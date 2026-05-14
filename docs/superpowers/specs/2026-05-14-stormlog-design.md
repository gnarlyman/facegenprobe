# StormLog — FaceGen storm-event logger for Oblivion

**Status:** Design approved 2026-05-14
**Audience:** implementer (next: writing-plans → executing-plans)
**Project root:** `D:/Modlists/_clones/StormLog/`

## Problem

FPS drops and eventual crashes during Oblivion play are suspected to correlate with FaceGen "storms" — bursts of repeated FaceGen processing for a small number of NPCs, typically those with null or invalid face-data records. Blockhead's existing retry-loop dedup (HeadOverride.cpp:786–817 in the gnarlyman fork) short-circuits the *same-NPC same-FGP consecutive* case, but:

- Only that one entry point is observed.
- Detection emits one OBSE-log line per burst; no aggregation, no per-NPC tally, no cross-session record.
- No way to confirm whether felt FPS drops actually correlate with storm events, or which NPC(s) drive them.

Without the data, the user has no actionable path for narrowing the search. Current modlist under test: MOO + non-overhaul mods only (OCO and OOO disabled for bisection).

## Goals

1. Identify which NPCs (FormID + EDID + source ESP) generate storm events during play.
2. Capture per-NPC counts and timing so post-session analysis can correlate felt FPS drops with logged events.
3. Surface storm bursts in-game in real time, but only when meaningful, so the player can mentally tag the moment.
4. Add no measurable rendering overhead and no new instability.

## Non-goals

- Not fixing storms. Pure data collection.
- Not modifying engine behavior. Hooks observe only.
- Not coupling to Blockhead. Runs with stock Blockhead, the gnarlyman fork, or no Blockhead.
- Not solving the upstream null-face-data root cause. That work is separate.

## Architecture

Standalone OBSE plugin (`StormLog.dll`). Independent of Blockhead. Three observation layers, each a thin detour-chained hook that increments counters and (when a burst boundary closes) emits an event to a lock-free ring buffer drained by a dedicated thread that writes a per-session CSV file.

```
   Engine                                  StormLog hot path
   ──────                                  ─────────────────
                                          ┌─ thread-local burst counter
   TESRace::GetFaceGenHeadParameters ──►  │  (no map, no lock, no alloc)
       (L1, main + various threads)       │
                                          ▼  on burst boundary
                                          ┌─ shared NPC-stats map
   QueuedHead_Run    @ 0x004353D0    ──►  │  (mutex + slab + LRU)
       (L3a, BSTaskThread)                │
                                          ▼  on event
                                          ┌─ mutex-protected ring buffer
   BSTaskThread_Runnable @ 0x00430DE0 ──► │  (lock-free, fixed 16 KiB)
       (L3b, BSTaskThread)                │
                                          ▼  drained by
                                             dedicated drain thread ─► CSV file
                                             console surface (on STORM event)
```

### Hook layers

| Layer | Symbol / address | Counted | Thread |
|---|---|---|---|
| L1  | `TESRace::GetFaceGenHeadParameters` (via `InstanceAbstraction::kTESRace_GetFaceGenHeadParameters` — same address Blockhead detours) | Per-NPC call count; per-NPC same-NPC+same-FGP retry trips with independent dedup state | Main / various |
| L3a | `QueuedHead_Run` @ `0x004353D0` | Per-NPC queued-head dispatches | `BSTaskThread` worker |
| L3b | `BSTaskThread_Runnable` @ `0x00430DE0` | Global tasks/sec throughput (no per-NPC dimension) | `BSTaskThread` worker |

Hooks install via OBSE Detours (`DetourAttach`). Because chains run last-installed-first, StormLog sees raw call rate before Blockhead's dedup short-circuits. Blockhead's dedup behaviour is unchanged.

**Install timing.** Blockhead installs its hooks in `OBSEPlugin_Load`. To guarantee StormLog's detours chain *in front of* Blockhead's regardless of OBSE plugin discovery order, StormLog installs all hooks in its `kMessage_PostPostLoad` handler — by which point all OBSE plugins (including Blockhead) have completed `OBSEPlugin_Load` and installed their own detours.

### Latency budget

The L1 hook fires on the same hot path Blockhead's dedup is built to short-circuit in ~10 ns/call (one pointer compare). A naive observer at 100–200 ns/call would dilute Blockhead's mitigation ~20× during storms — unacceptable.

Mitigations:
1. **Thread-local fast path.** Same-NPC + same-FGP within `iBurstWindowMs` (50 ms default) → bump thread-local burst counter only; no map access, no lock, no allocation. ~10–15 ns/call.
2. **Burst-end fold.** When the thread-local burst breaks, fold counters into the shared map under a single mutex acquisition and emit at most one event row.
3. **String caching.** EDID, mod source, cell name resolved exactly once per FormID on first-seen; never touched on hot path thereafter.
4. **No I/O on hot path.** Events go to a mutex-protected ring buffer; a dedicated drain thread does the `fwrite`/`fflush`.
5. **No heap allocations on hot path.** Pre-allocated slab of `iMaxTrackedNpcs` (4096 default) NpcStats entries; intrusive freelist; FormID-keyed open-addressed hash. Cap exceeded → stop tracking new NPCs, keep counting existing.
6. **L3 hooks** run on `BSTaskThread`, off the render thread. Cost there does not compete with frame production.

A microbenchmark target (`StormLogBench.exe`) runs the L1 path 1M times with and without an active burst, prints ns/call, and fails the build if it regresses >25% from baseline.

## Data model

### NpcStats slab entry

```c
struct NpcStats {
    UInt32 formID;            // key; 0 == free slot
    char   edid[64];          // cached on first-seen
    char   mod_source[32];    // ESP filename, cached on first-seen
    UInt64 first_seen_ms;     // GetTickCount64 at first observation
    UInt64 last_seen_ms;
    UInt32 total_l1_calls;
    UInt32 total_l1_retries;  // same-NPC same-FGP trips
    UInt32 total_l3_dispatch; // QueuedHead_Run fires
    UInt32 max_burst_per_sec; // peak observed call rate
    UInt64 last_console_ms;   // for console-surfacing cooldown
    UInt64 recent_ts[16];     // ring of recent call timestamps for burst-rate calc
    UInt8  recent_head;       // ring write index
    UInt8  free_next;         // freelist pointer (when slot is free)
};
```

Open-addressed `FormID → slab index` hash; linear probe; load factor capped at 0.75 (with `iMaxTrackedNpcs=4096`, hash table size = 8192). LRU eviction by `last_seen_ms` when the slab is full.

### Burst-rate detection

On L1 burst-end fold:
1. Acquire map mutex.
2. Look up / allocate NpcStats by FormID.
3. Push `now_ms` into `recent_ts[recent_head]`; advance head.
4. Count entries in `recent_ts` with `now_ms - ts <= 1000`. Update `max_burst_per_sec` if higher.
5. If count ≥ `iBurstThresholdPerSecond` and `now_ms - last_console_ms >= iConsoleCooldownMs`:
   - Emit `STORM` event row to ring.
   - If `bConsoleSurface=1`, call `Console::Print` (under mutex still held).
   - Update `last_console_ms`.
6. Otherwise: emit `BURST_END` event row to ring.
7. Release mutex.

L3 hooks emit `L3_DISPATCH` (L3a, per-NPC) and accumulate a global counter for L3b (no per-event row; included in `BURST_END` and `STORM` rows as `total_l3` context).

### CSV schema

Path: `Data/OBSE/Plugins/StormLog/YYYY-MM-DD_HHMMSS.csv`. Created lazily on first event.

Columns (header row written on file create):
```
time_ms, layer, event_type, npc_formid, npc_edid, mod_source,
cell_formid, burst_count, calls_in_window, total_calls_l1,
total_retries_l1, total_l3
```

- `time_ms` — `GetTickCount64()` at event time, milliseconds since plugin init
- `layer` — `L1` | `L3a` | `L3b`
- `event_type` — `FIRST_SEEN` | `BURST_END` | `STORM` | `L3_DISPATCH`
- `npc_formid` — 8-hex-digit FormID (uppercase, zero-padded)
- `npc_edid` — cached EDID or empty
- `mod_source` — owning ESP filename or empty
- `cell_formid` — current cell FormID at event time (`PlayerCell` lookup, cached for 1 s to avoid per-event cost) or empty if not in a cell
- `burst_count` — thread-local repeats folded by this event (L1 only; 0 elsewhere)
- `calls_in_window` — entries in `recent_ts` within last 1000 ms (L1 only)
- `total_calls_l1` — running per-NPC total
- `total_retries_l1` — running per-NPC total
- `total_l3` — running per-NPC L3a dispatch count

Newline = `\n`, UTF-8, no BOM. Quote-escape any field containing `,` or `"`.

### Drain thread

- Started on `OBSEMessagingInterface::kMessage_PostPostLoad` (same handler that installs hooks).
- Ring buffer of 16 KiB (~80 rows at ~200 B/row), protected by its own mutex separate from the NPC-map mutex.
- Producers acquire ring mutex briefly to copy a row in. Drain thread acquires it briefly to swap in an empty buffer (double-buffered), then writes the full one out without holding the lock.
- Sleep loop: swap buffers → `fwrite` accumulated rows → `fflush` if `iFlushIntervalMs` elapsed OR `iFlushEveryNEvents` written → sleep `iFlushIntervalMs / 4`.
- Stops on `OBSEMessagingInterface::kMessage_ExitGame`; flushes and closes file.
- Ring overflow → increment `s_dropped_events`; on next drain when space is free, write a `# DROPPED N events` row to CSV. Hot path never blocks.

### Console surfacing

On `STORM` event with `bConsoleSurface=1`:
```
[StormLog] STORM npc=000ABCDE (BanditBoss / OOO.esp) calls=12/1s totalRetries=47 cell=00012345
```
Per-NPC cooldown via `last_console_ms`. Format intentionally one line, parseable by eye and `grep`.

## INI

Path: `Data/OBSE/Plugins/StormLog.ini`. Parsed once on init via `GetPrivateProfileInt`. Missing file = all defaults. Unknown keys ignored.

```ini
[Logging]
bWriteCSV=1
bConsoleSurface=1
iFlushIntervalMs=1000
iFlushEveryNEvents=100

[Detection]
iBurstThresholdPerSecond=5
iConsoleCooldownMs=5000
iBurstWindowMs=50
iMaxTrackedNpcs=4096

[Hooks]
bEnableLayer1=1
bEnableLayer3a=1
bEnableLayer3b=1
```

## Error handling

- INI missing → defaults, log notice via `_MESSAGE`.
- CSV file open fails → disable `bWriteCSV` at runtime, log notice, console surfacing continues.
- Hook install fails on any layer → log which one, leave others installed; plugin keeps running degraded.
- Ring buffer overflow → drop + counter (covered in drain thread section).
- NPC map full → stop tracking new NPCs, log one notice; existing entries keep counting.
- Thread-local storage allocation failure (vanishingly rare on Win32) → call passes through unchanged, no logging for that thread.

No exceptions propagate across hook boundaries (hooks are `__stdcall` from the engine and cannot unwind safely).

## Build & deployment

- Repo: `D:/Modlists/_clones/StormLog/`, initialized as `git init` at design time.
- Build environment: VS 2026 + xOBSE 22 SDK, junction layout matching Blockhead:
  - `StormLog/Detours -> /d/Modlists/_clones/xOBSE/obse/Detours`
  - `StormLog/..` peer to `_clones/xOBSE/`, `_clones/OBSE-Plugin-Build-Dependencies/`
- Solution: `StormLog.sln` with three projects:
  - `StormLog.vcxproj` — main plugin DLL
  - `StormLogBench.vcxproj` — microbenchmark exe (Release-only)
  - `StormLogProbe.vcxproj` — L3-only probe DLL (Debug-only; validates hook addresses)
- Version: `0.1.0`, baked into `BuildInfo.h` via `BuildIncrement.jse` (mirrors Blockhead).
- MO2 deployment: new mod `StormLog` with `OBSE/Plugins/StormLog.dll` and `OBSE/Plugins/StormLog.ini`. Activate in Reborn-MOO-only test profile.

## Testing

1. **Probe-first validation gate.** Build `StormLogProbe.dll`, install in MO2, run a short play session. Confirm L3a and L3b addresses dispatch as expected (probe prints one line per fire). If either address is wrong, re-RE and update before continuing. **This is a gate — implementation does not proceed past hook install until probe passes.**
2. **Microbenchmark.** `StormLogBench.exe` runs the L1 hot path 1M iterations: cold map, warm map with no burst, warm map with active burst. Prints ns/call. Build fails if any number regresses >25% from a checked-in baseline (`bench-baseline.txt`).
3. **Replay unit test.** Feed a recorded sequence of `(formID, fgp_ptr, ts_ms)` tuples into the L1 logic in a unit-test harness; assert expected `BURST_END` / `STORM` event rows and final NpcStats. Runs in CI; no game required.
4. **Smoke test.** MOO-only profile, 10 min in Imperial City. Confirm: CSV file created, header present, rows parseable, no crash, no measurable FPS regression vs. plugin disabled (eyeball check, then if subtle, OBSE-runtime frametime monitor).
5. **Storm-induction test.** Use known storm-inducing NPC (Blockhead comments reference OOO VirtueRider `000700CC`). Re-enable OOO long enough to load near that NPC, confirm CSV captures a STORM event for that FormID. Disable OOO and resume MOO-only testing.

## Open implementation questions

- **`mod_source` resolution.** Probably `(formID >> 24) → plugins.txt index → ESP filename` via OBSE `GetNthModName`. If that doesn't exist directly, traverse `DataHandler::modList`. Implementer to confirm in the first iteration.
- **LRU eviction policy.** Evict by `last_seen_ms` (true LRU) vs. evict by `first_seen_ms` (FIFO). True LRU preferred for correctness; FIFO simpler. Default to true LRU; revisit only if mutex hold time becomes a problem.
- **L3a per-NPC attribution.** `QueuedHead_Run` takes the queued task struct as `this`; the contained `TESNPC*` lives at an offset to be confirmed during probe step. If attribution turns out to be unreliable, fall back to global L3a counter and drop `L3_DISPATCH` event type.

## Out of scope (for this spec)

- A Python summarizer / analyzer over the CSV. Defer until we have real data to know what summary is useful.
- Cross-session aggregation. Each session = one CSV; user merges by `cat *.csv` until a real tool is needed.
- Coupling with Blockhead's existing `RETRY-LOOP` log lines. They stay in the OBSE log unchanged; StormLog observes independently.

## Decision log

- **2026-05-14:** Standalone plugin chosen over Blockhead IPC coupling. Reason: keeps logging concern fully separate from stability fixes; works with stock Blockhead.
- **2026-05-14:** All three layers chosen over Layer-1-only. Reason: storm hypothesis is unconfirmed at the worker-thread layer; layered observation is the only way to tell which layer correlates with felt FPS drops.
- **2026-05-14:** CSV per session chosen over SQLite/JSON. Reason: greppable, diffable, importable into existing pandas/Excel tooling without new deps.
- **2026-05-14:** Burst-threshold console surface chosen over silent / per-event console. Reason: lets real-time correlation happen without flooding the console during cell loads.
