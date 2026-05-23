# StormReproAltStart — deterministic FaceGen-storm repro plugin

> **2026-05-14 PIVOT (Pattern 2 → Pattern 1).** The original design below overrode
> the Charactergen Stage 5 *result script* (Pattern 2). That requires recompiling
> SCDA bytecode, and research confirmed there is **no** headless Oblivion script
> compiler — only the CS/CSE GUI — and the CSE roundtrip structurally corrupts a
> new-worldspace ESP (drops rehomed refs, bloats masters). The plugin was rebuilt
> as **Pattern 1 (marker override, zero scripts)**: it rehomes the *marker the
> active Stage 5 script already moves the player to* into an isolated custom cell,
> alongside the rehomed VirtueRider ACHR/ACRE. No QUST override, no SCTX/SCDA, no
> CS/CSE/OBC. See the updated Architecture and Decision Log. The result is a
> single-master (`Oblivion.esm`) 1 KB ESP. Requirement: the active winning
> Charactergen Stage 5 must `player.moveto CGPlayerStartMarker` — so the PSMQD
> alt-start mod (which redirects to its own `PSAltStartMarker`) must be DISABLED
> in the test profile (user-chosen 2026-05-14). See `reference_oblivion_altstart_patterns`.

**Status:** Pattern 1 implemented 2026-05-14 (supersedes Pattern 2 design below)
**Audience:** implementer (next: writing-plans → executing-plans)
**Project root:** `D:/Modlists/Reborn/` (mod folder will live at `Reborn/mods/StormReproAltStart/`)
**Test profile:** `Reborn-Minimal`

## Problem

`StormLog` confirmed that vanilla Imperial Legion mounted patrols (around `000700CX` NPC_ templates, placed via ACHR `00070106` "VirtueRider" + ACRE `00070107` horse in Tamriel CELL `00023777`) generate FaceGen "storm" events that lower FPS and, with Blockhead's instability surface, can crash the game.

The bug currently has no consistent repro. Loading a save with the storm-prone NPC in the active cell **does not** restorm — the save serializes whatever FaceGen state was already produced, so the engine never re-runs the storming pipeline. Only a **fresh new game** triggers it.

Iterating on a fix or a deeper investigation requires running a full vanilla character-gen + prison cutscene each cycle (5+ minutes per attempt). The user also wants the storm-target NPC isolated — vanilla cell `00023777` has surrounding worldspace cells whose contents pollute the repro.

## Goals

1. **Fresh new game lands the player in an isolated cell containing exactly one mounted patrol NPC, in ≤ a few seconds.** This triggers the cell-load FaceGen path on that NPC immediately, producing a storm.
2. **No chargen UI between "New Game" click and player control.** Birthsign and class menus suppressed; race menu suppressed if possible, force-closed if not.
3. **Same exact vanilla NPC** (`ACHR 00070106` VirtueRider + `ACRE 00070107` horse) — not a clone, not `placeatme`, not a new NPC. The user has been firm on this constraint: the bug must reproduce in the engine path that the vanilla game uses, not a synthesized path.
4. **No mutation of anything outside the test profile's plugin scope** — the ESP is contained, can be toggled per-profile, leaves vanilla CGPlayerStartMarker untouched.

## Non-goals

- Not fixing the storm. Pure repro harness.
- Not preserving normal play. The mod is for the Reborn-Minimal test profile only; if it's left active in a play profile, the player will spawn in an empty test cell with a confused mounted patrol next to them. That's acceptable — the user enables it intentionally per repro session.
- Not testing every storm-target NPC. One vanilla mounted patrol (VirtueRider) is sufficient; the same mechanism extends trivially to more if needed.
- Not skipping the brief load of vanilla `ImperialDungeon01`. The engine spawns the player there before Stage 5 fires; that's intrinsic to the alt-start technique. The player sees ImperialDungeon01 for a fraction of a second before the teleport.

## Architecture — Pattern 1 (as built 2026-05-14)

Pure-override ESP, **no scripts of any kind**, single master `Oblivion.esm`, ~1 KB.

| Record | Type | Role |
|---|---|---|
| `StormReproWRLD` | WRLD (new) | Isolated custom worldspace |
| `StormReproCell` | CELL (new, exterior 0,0) | The repro cell |
| `00032AB5` | REFR (override) | Rehome vanilla `CGPlayerStartMarker` into `StormReproCell` — unmodified vanilla Stage 5 `player.moveto CGPlayerStartMarker` lands the player here |
| `00070106` | ACHR (override) | Rehome VirtueRider into `StormReproCell` |
| `00070107` | ACRE (override) | Rehome VirtueRider's horse into `StormReproCell` |

(Plus two empty vanilla cell-stub overrides — `0001FBB9`, `00023777` — auto-created by xEdit's `wbCopyElementToFile` as the rehomed refs' original-cell context. Harmless.)

No QUST override, no SCTX/SCDA, no temp SCPT, no OBC, no CSE, no `run_obc.py`/`strip_masters.py` (those artifacts are obsolete dead code). Built entirely via xEdit REPL (`build_esp.pas` + `run_build.py`). **Profile requirement:** PSMQD alt-start mod disabled so vanilla Stage 5 wins.

---

## Architecture (Pattern 2 — SUPERSEDED, retained for history)

One ESP, no scripts external to it, no DLL, no BSA. Three records do the work; everything else is supporting metadata.

```
  ImperialDungeon01 (vanilla)         StormReproCell (new ESP)
  ─────────────────────────           ─────────────────────────
  CGPlayerStartMarker  ──────▶        StormReproSpawnMarker (XMarkerHeading)
       (engine spawns                       ▲
        player here                         │ Player.MoveTo
        on new game)                        │
                                            │
                                            ▼
                                       VirtueRider ACHR 00070106
                                       (rehomed override of vanilla REFR)
                                       + horse ACRE 00070107
                                       (rehomed override)
```

### Records authored

| Record | Type | Role | Notes |
|---|---|---|---|
| `StormReproWRLD` | WRLD (new) | Custom single-cell worldspace | Avoids loading neighbor cells in Tamriel grid; isolation guarantee |
| `StormReproCell` | CELL (new, exterior 0,0) | The repro cell | Default flags; standard exterior |
| `StormReproSpawnMarker` | REFR (new, XMarkerHeading) | Player teleport target | Placed near center of StormReproCell |
| `00070106` | ACHR (override) | Rehome VirtueRider into StormReproCell | NAME→ImperialLegionRiderVirtue NPC_ unchanged; only Cell + DATA(position) change |
| `00070107` | ACRE (override) | Rehome VirtueRider's horse into StormReproCell | NAME→ImpLegionHorseVirtue CREA unchanged; only Cell + DATA change |
| `0002466E` | QUST (override of vanilla Charactergen) | Replace Stage 5 result script | New SCTX teleports player to StormReproSpawnMarker, skips MQ01 start, marks chargen done |

### Stage 5 result script (verbatim target)

```
setinchargen 1
set charactergen.fQuestDelayTime to .001
player.moveto StormReproSpawnMarker
setstage Charactergen 88
CloseAllMenus
```

Line-by-line:

- `setinchargen 1` — preserves vanilla "in chargen" flag, matches UESP CS wiki pattern. May be unnecessary; cheap to leave in.
- `set charactergen.fQuestDelayTime to .001` — minimizes the gap between stage 0 (engine sets) and stage 5 (this script). Standard alt-start technique.
- `player.moveto StormReproSpawnMarker` — the teleport. Engine pulls in StormReproCell, which contains the rehomed VirtueRider + horse REFRs. Cell-load triggers FaceGen pipeline on the patrol → storm fires.
- `setstage Charactergen 88` — marks chargen as fully complete. Prevents DLC couriers' `getstage CharacterGen >= 88` gates from blocking (per `feedback_chargen_gates_dlc_couriers.md`).
- `CloseAllMenus` — defensive close of any engine-fired race menu blip. OBSE 15+ function; no harm if no menus are open.

PSMQD's Stage 5 override (the empirically-validated template) is the reference; this version strips out everything we don't need (MQ01 start, prison NPC ignore-hits setup, PSCharactergen start, prison-clothes removal) and adds CloseAllMenus.

### Records explicitly NOT authored

- No override of `CGPlayerStartMarker` (REFR `00032AB5`). Vanilla marker stays in ImperialDungeon01; player spawns there first, gets teleported out within one gamemode tick.
- No override of Player ACHR (`00000014`).
- No new NPC records. No clone of `ImperialLegionRiderVirtue` NPC_. The bug is being reproduced in the actual vanilla pipeline that triggers it in the wild.
- No `placeatme` calls. Cell-load is the trigger; runtime spawning would bypass the path under test.

## Data flow (new-game sequence)

1. User clicks "New Game" in main menu.
2. Engine spawns player ACHR at vanilla `CGPlayerStartMarker` REFR position inside `ImperialDungeon01`. Cell loads briefly. (~milliseconds to ~1 second visible.)
3. Engine fires `ShowRaceMenu` if its hardcoded trigger happens before Stage 5 result script — open question, see Risks.
4. `Charactergen` quest auto-runs (engine-driven). Stage transitions 0 → 5 immediately due to vanilla `CharGenQuest` gamemode block (`if getstage CharacterGen == 0 setstage characterGen 5`).
5. **Stage 5 result script (ours) fires.** `player.moveto StormReproSpawnMarker` triggers cell load on `StormReproCell`. Engine begins FaceGen pipeline on `ACHR 00070106` (VirtueRider) and `ACRE 00070107` (horse).
6. `CloseAllMenus` closes the race menu if it was opened.
7. Storm event fires; StormLog (already running as OBSE plugin) captures it.

## Risks & open questions

### Risk: race menu may or may not fire

UESP CS wiki `PC_Starting_Location` says alt-start plugins leave the player "without the chance to change their appearance or set their birthsign or class" — implying the race menu doesn't fire when Stage 5 teleports the player out. But vanilla `CharGenQuest` has the comment `; bring up race menu first` immediately above the stage 0→5 transition, suggesting race menu is fired before Stage 5 runs.

`CloseAllMenus` is the safety net. Whether or not the race menu blips, our Stage 5 script closes it. Acceptable outcomes:

- **A:** Race menu never fires → player goes straight from "New Game" to standing in StormReproCell.
- **B:** Race menu fires for a frame, then `CloseAllMenus` closes it. Player picks default race (whatever was selected when the menu opens — usually Imperial). Storm fires.

Either is fine for repro. If `CloseAllMenus` turns out not to suppress the race menu (it's listed as an OBSE function for closing UI menus; the race menu is a UI menu), we accept one clickthrough per repro cycle.

### Risk: cell-load timing

The repro depends on the engine running FaceGen on the patrol when StormReproCell loads. If FaceGen is gated by load-screen completion or by some other state the alt-start path bypasses, storm may not fire.

Mitigation: Quickly verifiable post-build. If first test produces no storm, retry with the player teleport split into two ticks (load delay) — but this is unlikely; cell-load FaceGen is well-documented.

### Risk: rehomed REFR breaking AI packages

VirtueRider's AI packages reference Tamriel cells (patrol waypoints). When the ACHR lives in StormReproCell instead, those packages will fail to find their targets. The actor will idle in place. **This is fine for repro** — we only need the FaceGen pipeline to fire, not the AI to function. The storm hypothesis is about face-data load, not AI.

### Open question: which NPC

Spec assumes VirtueRider (ACHR `00070106`). The other 13 mounted patrols (ACHRs that reference NPC_ templates `000700C0`–`CB`) live in other Tamriel cells with their own FormIDs. We could rehome all 14 into StormReproCell for a fatter test, but starting with one keeps the test minimal. If VirtueRider alone doesn't storm reliably, expand to the full set.

## Testing

1. **Build the ESP** (xEdit REPL or Python ESP builder per `feedback_build_override_esp_python.md`).
2. **Deploy to MO2** at `mods/StormReproAltStart/StormReproAltStart.esp`. Enable in Reborn-Minimal.
3. **Confirm Charactergen Stage 5 override loads winning** — load `Reborn-Minimal` in xEdit, verify our ESP's override of `0002466E` is at the end of the override chain. (PSMQD's override is currently the only other; ours should outrank if loaded after.)
4. **Smoke test:** click "New Game". Expected outcomes:
   - ImperialDungeon01 briefly visible, < 1 second.
   - Maybe one race-menu blip; if so, click any race.
   - Player ends up standing in StormReproCell facing VirtueRider on horseback.
5. **StormLog smoke:** confirm StormLog's CSV captures storm events keyed to `000700CC` (ImperialLegionRiderVirtue NPC_) and `00070106` (VirtueRider ACHR) within first 5 seconds of spawn.
6. **Cycle test:** quit to main menu, click "New Game" again. Repro must work every cycle — no save-game caching of FaceGen state.

## Decision log

- **2026-05-14 (SUPERSEDED, then REVERSED):** Originally chose to override the Charactergen Stage 5 result script (Pattern 2) over a marker override. That decision drove the entire failed OBC/CSE pipeline. Reversed same day: research established no headless SCDA compiler exists and the CSE roundtrip corrupts new-worldspace ESPs. Pattern 1 (marker override) is the standard technique (UESP `PC_Starting_Location`, `WorldBuilding_101`) and, for a throwaway test harness in an isolated profile, the "don't mutate vanilla marker" objection does not apply (non-goal: preserving normal play). See `reference_oblivion_altstart_patterns`, `feedback_research_standard_technique_before_contraption`.
- **2026-05-14:** Pattern 1 must target the marker the *winning* Stage 5 script actually uses. In Reborn-Minimal the winner is PSMQD, which comments out `player.moveto CGPlayerStartMarker` and does `Player.MoveTo PSAltStartMarker` (PSMQD-owned REFR `01003F22`, in PSMQD cell `PSStartingShip`). To keep a single `Oblivion.esm` master, the user chose to **disable the PSMQD alt-start mod** in the test profile so vanilla Stage 5 wins and `player.moveto CGPlayerStartMarker` runs — making the single-master CGPlayerStartMarker rehome effective. (Rejected alt: override `PSAltStartMarker`, which would add PSMQD as a 2nd master.)
- **2026-05-14:** New worldspace rather than empty Tamriel cell. Reason: user constraint "I don't want existing stuff impacting"; Tamriel grid loads neighbor cells, custom worldspace doesn't.
- **2026-05-14:** Rehome vanilla VirtueRider ACHR/ACRE rather than clone the NPC_. Reason: user constraint "THE EXACT WAY VANILLA DOES, NOT A CLONE"; same FormIDs = same bug surface.
- **2026-05-14:** Include `CloseAllMenus` as race-menu safety net rather than research the exact engine trigger timing. Reason: empirical close is cheap; understanding when ShowRaceMenu auto-fires is research not aligned with the immediate goal (working repro plugin).
- **2026-05-14:** Single patrol (VirtueRider) for v1 rather than all 14. Reason: minimal repro; expand only if needed.

## Out of scope

- Tooling to automate "click New Game → measure storm". Manual click is fine for now.
- Repro for MOO ILR clusters (the transient FormIDs `00461B6X` etc. from StormLog memory). MOO isn't loaded in Reborn-Minimal; that's a follow-up if the vanilla repro doesn't reproduce the cluster-A symptoms.
- A clean-up flow for the test ESP. User loads it explicitly, knows what it does, removes it when done.
