# StormReproAltStart Implementation Plan

> **2026-05-14 — PLAN SUPERSEDED BY PATTERN 1.** Tasks 1–8 below (OBC/CSE SCDA
> recompile pipeline) are obsolete. Research proved no headless Oblivion script
> compiler exists and the CSE roundtrip corrupts new-worldspace ESPs. The plugin
> was rebuilt as a pure marker-override (Pattern 1). **Actual executed procedure:**
>
> 1. `research/storm_repro/build_esp.pas` (rewritten, Pattern 1): authors new
>    `StormReproWRLD` + `StormReproCell`; rehomes `CGPlayerStartMarker` REFR
>    `00032AB5` + VirtueRider ACHR `00070106` + horse ACRE `00070107` into the
>    cell. No QUST/SCTX/SCDA.
> 2. `python run_build.py` (xEdit REPL) → 1 KB single-master (`Oblivion.esm`) ESP.
>    Binary-verified: correct masters, all 3 refs rehomed, no scripts.
> 3. Installed to `mods/StormReproAltStart/` (+ `meta.ini`).
> 4. **Profile requirement:** disable the PSMQD alt-start mod in the test profile
>    so vanilla Charactergen Stage 5 (`player.moveto CGPlayerStartMarker`) wins.
> 5. Smoke test (manual, in MO2): New Game → player lands in `StormReproCell`
>    next to VirtueRider → StormLog CSV captures the FaceGen storm.
>
> Obsolete dead artifacts: `run_obc.py`, `strip_masters.py`, `stage5.sctx`,
> temp-SCPT logic. See spec Decision Log + `reference_oblivion_altstart_patterns`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a single .esp (`StormReproAltStart.esp`) that teleports the player from new-game into an isolated worldspace cell containing the vanilla `VirtueRider` mounted patrol (ACHR `00070106` + ACRE `00070107`), so a fresh New Game reliably triggers the FaceGen storm event for StormLog to capture.

**Architecture:** xEdit REPL (gnarlyman/TES5Edit feat/repl-mode) authors all records — new WRLD/CELL/spawn marker, rehome of vanilla ACHR/ACRE, and an override of vanilla `Charactergen` quest with new Stage 5 SCTX text. Bytecode (`SCDA`) is then recompiled by `oblivion-batch-compile` (OBC), an OBSE editor plugin that drives the CS-internal compiler from env vars. End-to-end is fully scriptable, no GUI clicks. The plugin is then installed as a Reborn-Minimal MO2 mod and activated.

**Tech Stack:** xEdit Pascal (JvInterpreter), Python (REPL client + orchestration), gnarlyman/oblivion-batch-compile, MO2 profile system.

**Spec:** `D:/Modlists/_clones/StormLog/docs/superpowers/specs/2026-05-14-storm-repro-altstart-design.md`

---

## File Structure

| Path | Role |
|---|---|
| `D:/Modlists/Reborn/research/storm_repro/probe_wrld.pas` | Empirical probe: can REPL author a new WRLD/CELL? |
| `D:/Modlists/Reborn/research/storm_repro/build_esp.pas` | Pascal authoring script (records only — no SCDA) |
| `D:/Modlists/Reborn/research/storm_repro/stage5.sctx` | Stage 5 result-script source (read by build_esp.pas) |
| `D:/Modlists/Reborn/research/storm_repro/run_build.py` | REPL orchestrator |
| `D:/Modlists/Reborn/research/storm_repro/run_obc.py` | OBC invocation orchestrator |
| `D:/Modlists/Reborn/research/storm_repro/verify_esp.pas` | Post-build verification script (records + SCTX + SCDA presence) |
| `D:/Modlists/Reborn/mods/StormReproAltStart/StormReproAltStart.esp` | Final installed .esp |
| `D:/Modlists/Reborn/profiles/Reborn-Minimal/plugins.txt` | Modify: append our esp at end |

The build pipeline is: `build_esp.pas` → ESP with SCTX-only → `run_obc.py` → ESP with compiled SCDA → install to MO2.

---

## Pre-flight: confirm environment

Run these from `D:/Modlists/Reborn/research/` before starting Task 1:

```bash
# REPL launcher present
ls "D:/Modlists/Reborn/research/usvfs-poc/UsvfsLauncher/bin/Release/net8.0-windows10.0.17763.0/UsvfsLauncher.exe"

# xEdit fork present
ls "D:/Modlists/Reborn/mods/TES4Edit/TES4Edit 4.1.5f/TES4Edit_patched.exe"

# OBC built
ls "D:/Modlists/_clones/oblivion-batch-compile/Build/Release/"
# expect: obse_oblivion-batch-compile.dll

# CSE present and OBSE editor mode works
ls "D:/Modlists/Reborn/Stock Game/Data/OBSE/Plugins/" | grep -i "construction\|oblivion-batch"

# StormLog deployed and observed working in Reborn-Minimal
ls "D:/Modlists/Reborn/mods/StormLog/Data/OBSE/Plugins/StormLog.dll"
```

If any of these is missing, fix that first before starting the plan.

---

### Task 1: Empirically probe REPL authoring of a new WRLD + CELL + REFR

We have not previously authored a brand-new WRLD via xEdit REPL. Per `feedback_test_before_design.md`, probe before designing. If the probe fails, we redirect to a different authoring strategy (CSE GUI) before sinking time into the full builder.

**Files:**
- Create: `D:/Modlists/Reborn/research/storm_repro/probe_wrld.pas`
- Create: `D:/Modlists/Reborn/research/storm_repro/run_probe.py`

- [ ] **Step 1: Create the storm_repro directory**

```bash
mkdir -p "D:/Modlists/Reborn/research/storm_repro"
```

- [ ] **Step 2: Write the probe Pascal script**

Create `D:/Modlists/Reborn/research/storm_repro/probe_wrld.pas`:

```pascal
unit ProbeWrldAuthoring;

(*
  Empirical probe: can REPL author a new WRLD with a child CELL with a
  child REFR? If yes, we can build the full repro ESP. If no, redirect
  to CSE-GUI authoring.

  Strategy: AddNewFileName + Add new WRLD + Add new CELL inside WRLD
  block + Add new REFR inside CELL. Save and inspect on disk.
*)

interface
implementation

var g_path: string;

function JsonEsc(const s: string): string;
begin
  Result := StringReplace(s,      '\', '\\',   [rfReplaceAll]);
  Result := StringReplace(Result, '"', '\"',   [rfReplaceAll]);
end;

procedure Emit(const s: string);
begin
  EmitJSONLine(g_path, '"' + JsonEsc(s) + '"');
end;

function Initialize: integer;
var
  target, oblEsm, wrld, cell, refr, xmarker: IInterface;
  i: integer;
  ok: boolean;
begin
  Result := 0;
  g_path := OpenOutput('probe_wrld.jsonl');

  (* Create new ESP *)
  target := AddNewFileName('StormReproProbe.esp');
  if not Assigned(target) then begin
    Emit('FATAL: AddNewFileName returned nil');
    WriteSummary('{"out":"' + JsonEsc(g_path) + '","ok":false}');
    Exit;
  end;
  Emit('Created ESP: ' + GetFileName(target));

  (* Pre-add Oblivion.esm as master *)
  for i := 0 to FileCount - 1 do
    if SameText(GetFileName(FileByLoadOrder(i)), 'Oblivion.esm') then begin
      oblEsm := FileByLoadOrder(i);
      Break;
    end;
  if not Assigned(oblEsm) then begin
    Emit('FATAL: Oblivion.esm not found in load order');
    WriteSummary('{"out":"' + JsonEsc(g_path) + '","ok":false}');
    Exit;
  end;
  AddMasterIfMissing(target, 'Oblivion.esm');
  Emit('Added Oblivion.esm as master');

  (* Probe: can we add a top-level WRLD group + WRLD record? *)
  ok := false;
  try
    wrld := Add(target, 'WRLD', True);
    if Assigned(wrld) then begin
      SetElementEditValues(wrld, 'EDID', 'StormReproWRLDProbe');
      Emit('Added WRLD record. FID=' + IntToHex(GetLoadOrderFormID(wrld), 8) +
           ' EDID=' + GetElementEditValues(wrld, 'EDID'));
      ok := true;
    end else
      Emit('Add(WRLD) returned nil');
  except
    on E: Exception do
      Emit('Add(WRLD) EXC: ' + E.Message);
  end;

  if ok then begin
    (* Probe: can we add a CELL as child of WRLD? *)
    try
      cell := Add(wrld, 'CELL', True);
      if Assigned(cell) then begin
        SetElementEditValues(cell, 'EDID', 'StormReproCellProbe');
        Emit('Added CELL. FID=' + IntToHex(GetLoadOrderFormID(cell), 8) +
             ' EDID=' + GetElementEditValues(cell, 'EDID'));
      end else
        Emit('Add(WRLD\\CELL) returned nil');
    except
      on E: Exception do
        Emit('Add(WRLD\\CELL) EXC: ' + E.Message);
    end;
  end;

  (* Probe: can we add a REFR child to that cell pointing at XMarkerHeading? *)
  if Assigned(cell) then begin
    xmarker := RecordByHexFormID('00000034');  (* XMarkerHeading STAT *)
    if not Assigned(xmarker) then
      Emit('XMarkerHeading STAT 00000034 not found')
    else
      try
        refr := Add(cell, 'REFR', True);
        if Assigned(refr) then begin
          SetElementEditValues(refr, 'EDID', 'StormReproSpawnMarkerProbe');
          SetElementNativeValues(refr, 'NAME', GetLoadOrderFormID(xmarker));
          Emit('Added REFR. FID=' + IntToHex(GetLoadOrderFormID(refr), 8) +
               ' NAME=' + GetElementEditValues(refr, 'NAME'));
        end else
          Emit('Add(CELL\\REFR) returned nil');
      except
        on E: Exception do
          Emit('Add(CELL\\REFR) EXC: ' + E.Message);
      end;
  end;

  (* Save the probe ESP to disk via xEdit's save path. Returns path
     where ESP was written (USVFS scratch in REPL mode). *)
  try
    SaveFile(target);
    Emit('SaveFile succeeded');
  except
    on E: Exception do
      Emit('SaveFile EXC: ' + E.Message);
  end;

  WriteSummary('{"out":"' + JsonEsc(g_path) + '","ok":' +
    LowerCase(BoolToStr(ok, True)) + '}');
end;

end.
```

- [ ] **Step 3: Write the probe Python orchestrator**

Create `D:/Modlists/Reborn/research/storm_repro/run_probe.py`:

```python
"""Run probe_wrld.pas against Reborn-Minimal and report results."""
import sys, json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from repl_client import REPLSession

XEDIT = r"D:\Modlists\Reborn\mods\TES4Edit\TES4Edit 4.1.5f\TES4Edit_patched.exe"
OUT = Path(__file__).parent / "probe_wrld.txt"

with REPLSession(profile="Reborn-Minimal", xedit_exe=XEDIT) as repl:
    r = repl.exec_path(Path(__file__).parent / "probe_wrld.pas")

print(f"envelope_ok={r.envelope.get('ok')}")
err = r.envelope.get("error")
if err: print(f"error={err!r}")
summary = r.envelope.get("summary")
print(f"summary={summary!r}")

src_path = None
if isinstance(summary, dict) and "out" in summary:
    src_path = Path(summary["out"])
if src_path and src_path.exists():
    lines = []
    for raw in src_path.read_text(encoding="utf-8", errors="replace").splitlines():
        try: lines.append(json.loads(raw))
        except json.JSONDecodeError: lines.append(raw)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT} ({len(lines)} lines)")
```

- [ ] **Step 4: Run the probe**

```bash
cd "D:/Modlists/Reborn/research/storm_repro" && python run_probe.py
```

Expected: `envelope_ok=True`, `summary['ok']=True`, and `probe_wrld.txt` shows successful WRLD + CELL + REFR adds with non-zero new FormIDs.

- [ ] **Step 5: Decision gate based on probe result**

If `envelope_ok=False` or any "EXC" / "returned nil" line appears: STOP. The xEdit REPL API doesn't support authoring brand-new WRLD/CELL/REFR records the way we need. Pivot to CSE-GUI authoring — open the construction set, manually build the WRLD/CELL/Marker, override the patrol REFRs, save. Skip Tasks 2–4 and resume at Task 5 with a manually-authored `StormReproAltStart.esp` placed at `D:/Modlists/Reborn/research/storm_repro/StormReproAltStart.esp`.

If probe succeeded: continue to Task 2.

- [ ] **Step 6: Commit probe results**

```bash
cd "D:/Modlists/Reborn/research/storm_repro" && git add probe_wrld.pas run_probe.py probe_wrld.txt
```

Note: research/ may or may not be a git repo. If not, skip; if yes, commit with message: `probe: verify xEdit REPL can author new WRLD/CELL/REFR`.

---

### Task 2: Write the Stage 5 SCTX source file

The result script's compiled SCDA needs a separate compile pass via OBC. The SCTX itself is plain text; we author it in a standalone file so OBC can find and recompile it.

**Files:**
- Create: `D:/Modlists/Reborn/research/storm_repro/stage5.sctx`

- [ ] **Step 1: Write the SCTX source**

Create `D:/Modlists/Reborn/research/storm_repro/stage5.sctx` with exactly this content (no trailing whitespace, LF line endings):

```
setinchargen 1
set charactergen.fQuestDelayTime to .001
player.moveto StormReproSpawnMarker
setstage Charactergen 88
CloseAllMenus
```

- [ ] **Step 2: Confirm file written correctly**

```bash
cat "D:/Modlists/Reborn/research/storm_repro/stage5.sctx"
```

Expected output: exactly the 5 lines above.

---

### Task 3: Write the ESP authoring Pascal script

This script runs in xEdit REPL and authors every record in the ESP except SCDA recompilation (which OBC handles in Task 5).

**Files:**
- Create: `D:/Modlists/Reborn/research/storm_repro/build_esp.pas`

- [ ] **Step 1: Write build_esp.pas**

Create `D:/Modlists/Reborn/research/storm_repro/build_esp.pas`:

```pascal
unit BuildStormReproESP;

(*
  Author StormReproAltStart.esp from scratch via xEdit REPL.

  Records authored:
    1. New WRLD "StormReproWRLD"
    2. New CELL "StormReproCell" under that WRLD (exterior 0,0)
    3. New REFR "StormReproSpawnMarker" (XMarkerHeading) in StormReproCell
    4. Override of ACHR 00070106 (VirtueRider) — rehome to StormReproCell
    5. Override of ACRE 00070107 (VirtueRider's horse) — rehome to StormReproCell
    6. Override of QUST 0002466E (vanilla Charactergen) — Stage 5 SCTX
       replaced with stage5.sctx contents. SCDA NOT compiled here; OBC
       handles it in a separate pass.

  Stage 5 SCTX text is read from
    D:/Modlists/Reborn/research/storm_repro/stage5.sctx
*)

interface
implementation

const
  STAGE5_SCTX_PATH = 'D:\Modlists\Reborn\research\storm_repro\stage5.sctx';
  SPAWN_X = '0.0';
  SPAWN_Y = '0.0';
  SPAWN_Z = '5000.0';   (* slightly above ground to avoid fall-through *)
  PATROL_X = '512.0';   (* 512 units in front of player *)
  PATROL_Y = '0.0';
  PATROL_Z = '5000.0';

var g_path: string;

function JsonEsc(const s: string): string;
begin
  Result := StringReplace(s,      '\', '\\',   [rfReplaceAll]);
  Result := StringReplace(Result, '"', '\"',   [rfReplaceAll]);
end;

procedure Emit(const s: string);
begin
  EmitJSONLine(g_path, '"' + JsonEsc(s) + '"');
end;

function ReadSCTXFromFile: string;
var
  sl: TStringList;
begin
  sl := TStringList.Create;
  try
    sl.LoadFromFile(STAGE5_SCTX_PATH);
    Result := sl.Text;
  finally
    sl.Free;
  end;
end;

function FindFileByName(const name: string): IInterface;
var i: integer;
begin
  Result := nil;
  for i := 0 to FileCount - 1 do
    if SameText(GetFileName(FileByLoadOrder(i)), name) then begin
      Result := FileByLoadOrder(i);
      Exit;
    end;
end;

function Initialize: integer;
var
  target, oblEsm, wrld, cellGroup, cell, refr, xmarker,
  vanillaAchr, vanillaAcre, achrOvr, acreOvr,
  vanillaCgen, cgenOvr, stages, stage, items, item, schr: IInterface;
  i, j: integer;
  spawnMarkerFid: cardinal;
  sctxText, sIdx: string;
begin
  Result := 0;
  g_path := OpenOutput('build_esp.jsonl');

  (* ---------- 1. Create target ESP, master it on Oblivion.esm ---------- *)
  target := AddNewFileName('StormReproAltStart.esp');
  if not Assigned(target) then begin
    Emit('FATAL: AddNewFileName failed');
    WriteSummary('{"out":"' + JsonEsc(g_path) + '","ok":false}');
    Exit;
  end;
  oblEsm := FindFileByName('Oblivion.esm');
  if not Assigned(oblEsm) then begin
    Emit('FATAL: Oblivion.esm not in load order');
    Exit;
  end;
  AddMasterIfMissing(target, 'Oblivion.esm');
  Emit('Created StormReproAltStart.esp with master Oblivion.esm');

  (* ---------- 2. Author new WRLD ---------- *)
  wrld := Add(target, 'WRLD', True);
  if not Assigned(wrld) then begin
    Emit('FATAL: Add(WRLD) failed');
    Exit;
  end;
  SetElementEditValues(wrld, 'EDID', 'StormReproWRLD');
  SetElementEditValues(wrld, 'FULL', 'Storm Repro');
  Emit('WRLD created: ' + IntToHex(GetLoadOrderFormID(wrld), 8));

  (* ---------- 3. Author new CELL ---------- *)
  cell := Add(wrld, 'CELL', True);
  if not Assigned(cell) then begin
    Emit('FATAL: Add(CELL) failed');
    Exit;
  end;
  SetElementEditValues(cell, 'EDID', 'StormReproCell');
  SetElementEditValues(cell, 'FULL', 'Storm Repro Cell');
  (* Exterior cell at grid 0,0. Cell flags: 0 (no special flags) *)
  SetElementEditValues(cell, 'DATA', '0');
  SetElementEditValues(cell, 'XCLC\Grid X', '0');
  SetElementEditValues(cell, 'XCLC\Grid Y', '0');
  Emit('CELL created: ' + IntToHex(GetLoadOrderFormID(cell), 8));

  (* ---------- 4. Author spawn marker REFR ---------- *)
  xmarker := RecordByHexFormID('00000034');  (* XMarkerHeading STAT *)
  refr := Add(cell, 'REFR', True);
  SetElementEditValues(refr, 'EDID', 'StormReproSpawnMarker');
  SetElementNativeValues(refr, 'NAME', GetLoadOrderFormID(xmarker));
  SetElementEditValues(refr, 'DATA\Position\X', SPAWN_X);
  SetElementEditValues(refr, 'DATA\Position\Y', SPAWN_Y);
  SetElementEditValues(refr, 'DATA\Position\Z', SPAWN_Z);
  SetElementEditValues(refr, 'DATA\Rotation\X', '0');
  SetElementEditValues(refr, 'DATA\Rotation\Y', '0');
  SetElementEditValues(refr, 'DATA\Rotation\Z', '0');
  spawnMarkerFid := GetLoadOrderFormID(refr);
  Emit('Spawn marker REFR: ' + IntToHex(spawnMarkerFid, 8));

  (* ---------- 5. Override VirtueRider ACHR + rehome to new cell ---------- *)
  vanillaAchr := RecordByHexFormID('00070106');
  if not Assigned(vanillaAchr) then begin
    Emit('FATAL: ACHR 00070106 not found');
    Exit;
  end;
  achrOvr := wbCopyElementToFile(vanillaAchr, target, False, True);
  if not Assigned(achrOvr) then begin
    Emit('FATAL: wbCopyElementToFile(ACHR) failed');
    Exit;
  end;
  (* Move into our cell. The Cell parent is set by xEdit-internal placement
     in the cell GRUP when we set element values that bind to the cell. *)
  SetElementNativeValues(achrOvr, 'Cell', GetLoadOrderFormID(cell));
  SetElementEditValues(achrOvr, 'DATA\Position\X', PATROL_X);
  SetElementEditValues(achrOvr, 'DATA\Position\Y', PATROL_Y);
  SetElementEditValues(achrOvr, 'DATA\Position\Z', PATROL_Z);
  Emit('VirtueRider ACHR rehomed: ' + IntToHex(GetLoadOrderFormID(achrOvr), 8));

  (* ---------- 6. Override VirtueRider's horse ACRE ---------- *)
  vanillaAcre := RecordByHexFormID('00070107');
  if not Assigned(vanillaAcre) then begin
    Emit('FATAL: ACRE 00070107 not found');
    Exit;
  end;
  acreOvr := wbCopyElementToFile(vanillaAcre, target, False, True);
  SetElementNativeValues(acreOvr, 'Cell', GetLoadOrderFormID(cell));
  SetElementEditValues(acreOvr, 'DATA\Position\X', PATROL_X);
  SetElementEditValues(acreOvr, 'DATA\Position\Y', PATROL_Y);
  SetElementEditValues(acreOvr, 'DATA\Position\Z', PATROL_Z);
  Emit('VirtueRider horse ACRE rehomed: ' + IntToHex(GetLoadOrderFormID(acreOvr), 8));

  (* ---------- 7. Override Charactergen QUST, modify Stage 5 SCTX ---------- *)
  vanillaCgen := RecordByHexFormID('0002466E');
  if not Assigned(vanillaCgen) then begin
    Emit('FATAL: Charactergen QUST not found');
    Exit;
  end;
  cgenOvr := wbCopyElementToFile(vanillaCgen, target, False, True);
  if not Assigned(cgenOvr) then begin
    Emit('FATAL: wbCopyElementToFile(Charactergen) failed');
    Exit;
  end;

  (* Find Stage 5 LogEntry, replace its Result Script SCTX *)
  sctxText := ReadSCTXFromFile;
  stages := ElementByName(cgenOvr, 'Stages');
  if not Assigned(stages) then begin
    Emit('FATAL: Charactergen has no Stages');
    Exit;
  end;
  for i := 0 to ElementCount(stages) - 1 do begin
    stage := ElementByIndex(stages, i);
    sIdx := GetElementEditValues(stage, 'INDX');
    if sIdx = '5' then begin
      items := ElementByName(stage, 'Log Entries');
      if Assigned(items) and (ElementCount(items) > 0) then begin
        item := ElementByIndex(items, 0);
        schr := ElementByName(item, 'Result Script');
        if Assigned(schr) then begin
          SetElementEditValues(schr, 'SCTX', sctxText);
          Emit('Charactergen Stage 5 SCTX overwritten (length=' +
               IntToStr(Length(sctxText)) + ')');
        end else
          Emit('FATAL: Stage 5 has no Result Script');
      end;
      Break;
    end;
  end;

  Emit('Charactergen QUST overridden: ' +
       IntToHex(GetLoadOrderFormID(cgenOvr), 8));

  (* ---------- 8. Save the ESP ---------- *)
  SaveFile(target);
  Emit('SaveFile complete');

  WriteSummary('{"out":"' + JsonEsc(g_path) + '","ok":true,"esp_basename":"StormReproAltStart.esp"}');
end;

end.
```

- [ ] **Step 2: Commit the SCTX file and build script**

```bash
cd "D:/Modlists/Reborn/research/storm_repro"
# only if this dir is part of a git repo; otherwise skip
```

---

### Task 4: Write the REPL orchestrator + run the build

**Files:**
- Create: `D:/Modlists/Reborn/research/storm_repro/run_build.py`

- [ ] **Step 1: Write run_build.py**

Create `D:/Modlists/Reborn/research/storm_repro/run_build.py`:

```python
"""Author StormReproAltStart.esp via xEdit REPL.

The ESP is written by xEdit to USVFS scratch first
(C:/Users/<user>/AppData/Local/Temp/xedit-repl-scratch-<pid>/Data/).
After the REPL run, we copy it from scratch to the project
output directory.
"""
import sys, json, shutil, glob, os
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from repl_client import REPLSession

XEDIT = r"D:\Modlists\Reborn\mods\TES4Edit\TES4Edit 4.1.5f\TES4Edit_patched.exe"
OUT_LOG = Path(__file__).parent / "build_esp.txt"
ESP_BASENAME = "StormReproAltStart.esp"
ESP_DEST = Path(__file__).parent / ESP_BASENAME

with REPLSession(profile="Reborn-Minimal", xedit_exe=XEDIT) as repl:
    r = repl.exec_path(Path(__file__).parent / "build_esp.pas")

print(f"envelope_ok={r.envelope.get('ok')}")
err = r.envelope.get("error")
if err: print(f"error={err!r}")
summary = r.envelope.get("summary")
print(f"summary={summary!r}")

# Dump readable log
src_path = None
if isinstance(summary, dict) and "out" in summary:
    src_path = Path(summary["out"])
if src_path and src_path.exists():
    lines = []
    for raw in src_path.read_text(encoding="utf-8", errors="replace").splitlines():
        try: lines.append(json.loads(raw))
        except json.JSONDecodeError: lines.append(raw)
    OUT_LOG.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote log: {OUT_LOG} ({len(lines)} lines)")

# Find the saved ESP in scratch and copy to project dir
scratch_root = Path(os.environ.get("TEMP", r"C:/Users/" + os.environ.get("USERNAME","") + "/AppData/Local/Temp"))
candidates = list(scratch_root.glob(f"xedit-repl-scratch-*/Data/{ESP_BASENAME}"))
if not candidates:
    candidates = list(scratch_root.glob(f"xedit-repl-scratch-*/**/{ESP_BASENAME}"))
if not candidates:
    print(f"FATAL: {ESP_BASENAME} not found under any xedit-repl-scratch-* directory")
    sys.exit(1)

# Take the most recently modified
src_esp = max(candidates, key=lambda p: p.stat().st_mtime)
shutil.copy2(src_esp, ESP_DEST)
print(f"copied {src_esp} -> {ESP_DEST} ({ESP_DEST.stat().st_size} bytes)")
```

- [ ] **Step 2: Run the build**

```bash
cd "D:/Modlists/Reborn/research/storm_repro" && python run_build.py
```

Expected: `envelope_ok=True`, `summary={'ok': True, 'esp_basename': 'StormReproAltStart.esp', ...}`, and `StormReproAltStart.esp` exists in the storm_repro directory.

- [ ] **Step 3: Sanity-check the file**

```bash
ls -la "D:/Modlists/Reborn/research/storm_repro/StormReproAltStart.esp"
```

Expected: file exists, size > 1 KB (probably 2-5 KB for our handful of records).

- [ ] **Step 4: If build failed, capture diagnostic**

If `envelope_ok=False` or the ESP didn't materialize, read the log:

```bash
cat "D:/Modlists/Reborn/research/storm_repro/build_esp.txt"
```

Diagnose the FATAL line, fix `build_esp.pas`, re-run. Common failures and fixes:

- `Add(WRLD) failed` → xEdit refused to add a top-level record. Check probe (Task 1) — should have caught this. If probe passed but build fails: an existing WRLD signature filter is blocking. Try `Add(target, 'WRLD', True)` with a different aAsNew value (False), or report to user.
- `wbCopyElementToFile(ACHR) failed` → likely a master-list mismatch. Verify `AddMasterIfMissing(target, 'Oblivion.esm')` succeeded earlier. Re-run.
- `Stage 5 has no Result Script` → the override's stage structure didn't copy fully. Try `wbCopyElementToFile(vanillaCgen, target, False, True)` with the as-new flag flipped.

---

### Task 5: Compile Stage 5 SCDA via oblivion-batch-compile

The ESP from Task 4 has updated SCTX but stale (vanilla) SCDA on Charactergen Stage 5. OBC drives the CS-internal compiler to regenerate SCDA from SCTX.

**Files:**
- Create: `D:/Modlists/Reborn/research/storm_repro/run_obc.py`

- [ ] **Step 1: Read OBC's env-var contract**

```bash
head -100 "D:/Modlists/_clones/oblivion-batch-compile/README.md"
```

Read the "Workflow" and "Usage" sections to confirm the env-var names. Expected vars (per `feedback_obc_troubleshooting.md`):

- `OBC_TARGET_PLUGIN` — basename of the .esp to load and recompile in
- `OBC_TARGET_FORMID` — FormID of the script-bearing record (optional; if omitted, recompiles all scripts)
- `OBC_LOG_PATH` — where to write the result JSON

(Names per the README; double-check before invoking.)

- [ ] **Step 2: Write run_obc.py**

The Charactergen QUST has a quest script (`CharGenQuest` SCPT, FID `0002480B`) and per-stage result scripts. The stage 5 result script doesn't have its own FormID — it's a sub-element of the QUST. OBC's target is normally a SCPT FormID. For QUST stage scripts, OBC supports the QUST FormID itself per `feedback_obc_troubleshooting.md`.

Per the memory:
- Use Pattern A: target = QUST FormID (`0002466E`). OBC recompiles all stage result scripts.

Create `D:/Modlists/Reborn/research/storm_repro/run_obc.py`:

```python
"""Run oblivion-batch-compile to regenerate Stage 5 SCDA in StormReproAltStart.esp.

Workflow:
  1. Copy our SCTX-only ESP into the Reborn-Minimal Data directory so CSE
     can load it (CSE works in the live game's Data, not USVFS scratch).
  2. Invoke CSE (TESConstructionSetCSE.exe or CS.exe with OBSE) under OBC's
     env-var contract. OBC loads the plugin, recompiles, saves, exits.
  3. Verify the result JSON shows sha256-before != sha256-after.
  4. Copy the recompiled ESP back to the project dir.

Important: per feedback_cse_save_loaded_esps_as_masters.md, set
SaveLoadedESPsAsMasters = 0 in CSE's INI before this run to prevent
the ESP from being bloated with extra masters on save.
"""
import os, sys, json, subprocess, shutil, hashlib
from pathlib import Path

PROJECT_DIR     = Path(__file__).parent
ESP_BASENAME    = "StormReproAltStart.esp"
ESP_PROJECT     = PROJECT_DIR / ESP_BASENAME
DATA_DIR        = Path(r"D:/Modlists/Reborn/Stock Game/Data")
ESP_IN_DATA     = DATA_DIR / ESP_BASENAME
OBC_LOG         = PROJECT_DIR / "obc_result.json"

# Find CSE/CS executable
CSE_CANDIDATES = [
    Path(r"D:/Modlists/Reborn/Stock Game/TESConstructionSetCSE.exe"),
    Path(r"D:/Modlists/Reborn/Stock Game/TESConstructionSet.exe"),
    Path(r"D:/Modlists/Reborn/Stock Game/obse_editor_loader.exe"),
]
CS_EXE = next((p for p in CSE_CANDIDATES if p.exists()), None)
if CS_EXE is None:
    print(f"FATAL: no CS/CSE exe found among {CSE_CANDIDATES}")
    sys.exit(1)
print(f"Using CS exe: {CS_EXE}")

def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

# Step 1: copy ESP into Data
shutil.copy2(ESP_PROJECT, ESP_IN_DATA)
before_hash = sha256(ESP_IN_DATA)
print(f"copied to Data; sha256-before={before_hash}")

# Step 2: invoke OBC via env vars
env = os.environ.copy()
env["OBC_TARGET_PLUGIN"] = ESP_BASENAME
env["OBC_TARGET_FORMID"] = "0002466E"  # vanilla Charactergen QUST
env["OBC_LOG_PATH"]      = str(OBC_LOG)

# CSE/CS must run with cwd = game install dir (where Oblivion.esm sits)
result = subprocess.run(
    [str(CS_EXE)],
    cwd=str(CS_EXE.parent),
    env=env,
    capture_output=True,
    text=True,
    timeout=120,
)
print(f"return code: {result.returncode}")
print(f"stdout (tail):\n{result.stdout[-1000:]}")
print(f"stderr (tail):\n{result.stderr[-1000:]}")

# Step 3: read OBC result JSON
if not OBC_LOG.exists():
    print(f"FATAL: OBC_LOG not produced at {OBC_LOG}")
    sys.exit(1)
log = json.loads(OBC_LOG.read_text(encoding="utf-8"))
print(f"OBC result: {json.dumps(log, indent=2)}")
if not log.get("saved", False):
    print("FATAL: OBC reports saved=False")
    sys.exit(1)

after_hash = sha256(ESP_IN_DATA)
print(f"sha256-after={after_hash}")
if before_hash == after_hash:
    print("WARNING: ESP hash unchanged — SCDA may not have been recompiled.")
    print("Possible causes: SCTX identical to existing SCDA decompile;")
    print("                 OBC didn't find the right script in the QUST;")
    print("                 OBC silently failed (check stdout/stderr above).")
    sys.exit(2)

# Step 4: copy back to project dir
shutil.copy2(ESP_IN_DATA, ESP_PROJECT)
print(f"copied back to {ESP_PROJECT} ({ESP_PROJECT.stat().st_size} bytes)")
```

- [ ] **Step 3: Pre-flight CSE INI**

Per `feedback_cse_save_loaded_esps_as_masters.md`, set the SaveLoadedESPsAsMasters flag to 0 first.

```bash
# Find CSE INI
ls "D:/Modlists/Reborn/Stock Game/Data/OBSE/Plugins/ConstructionSetExtender.ini" 2>&1 || \
  ls "D:/Modlists/Reborn/Stock Game/Construction Set Extender.ini" 2>&1
```

Edit whichever exists. Find the line:

```
SaveLoadedESPsAsMasters=1
```

Change to:

```
SaveLoadedESPsAsMasters=0
```

If the file doesn't have that line, add it under the `[General]` section (or wherever CSE keeps general settings — confirm by reading the file first).

- [ ] **Step 4: Run OBC**

```bash
cd "D:/Modlists/Reborn/research/storm_repro" && python run_obc.py
```

Expected: returncode 0, `OBC result` JSON shows `saved=true` and `sha256-before != sha256-after`. The ESP at `D:/Modlists/Reborn/research/storm_repro/StormReproAltStart.esp` is now updated with compiled SCDA.

- [ ] **Step 5: If OBC failed, troubleshoot per `feedback_obc_troubleshooting.md`**

Common failures:
- `OBC_LOG not produced`: OBC plugin didn't load. Check that the OBC DLL is in `Stock Game/Data/OBSE/Plugins/` and the CSE editor mode is invoked correctly. CSE's load may need `obse_loader.exe -editor` or the OBSE-aware launcher.
- `saved=false` in result JSON: compile error in the script. The result JSON will include compiler diagnostics. Fix the SCTX in `stage5.sctx` and re-run.
- `sha256 unchanged`: OBC didn't actually rewrite. Check that OBC matched the right script in the QUST — try with no OBC_TARGET_FORMID to recompile everything.
- Silent USVFS case mismatch (memory): ensure the ESP basename casing in `OBC_TARGET_PLUGIN` matches the on-disk file exactly.

---

### Task 6: Verify ESP via xEdit

Confirm the ESP looks right before installing it.

**Files:**
- Create: `D:/Modlists/Reborn/research/storm_repro/verify_esp.pas`
- Create: `D:/Modlists/Reborn/research/storm_repro/run_verify.py`

- [ ] **Step 1: Write verify_esp.pas**

Create `D:/Modlists/Reborn/research/storm_repro/verify_esp.pas`:

```pascal
unit VerifyStormReproESP;

(*
  Load StormReproAltStart.esp and verify:
   - 6 record overrides/authored as expected
   - Stage 5 SCTX matches stage5.sctx
   - Stage 5 SCDA non-empty (size > 0)
   - ACHR 00070106 / ACRE 00070107 overrides point at our new cell
*)

interface
implementation

var g_path: string;

function JsonEsc(const s: string): string;
begin
  Result := StringReplace(s,      '\', '\\',   [rfReplaceAll]);
  Result := StringReplace(Result, '"', '\"',   [rfReplaceAll]);
end;

procedure Emit(const s: string);
begin
  EmitJSONLine(g_path, '"' + JsonEsc(s) + '"');
end;

function FindFileByName(const name: string): IInterface;
var i: integer;
begin
  Result := nil;
  for i := 0 to FileCount - 1 do
    if SameText(GetFileName(FileByLoadOrder(i)), name) then begin
      Result := FileByLoadOrder(i);
      Exit;
    end;
end;

function Initialize: integer;
var
  ours, rec, stage, items, item, schr: IInterface;
  i, j: integer;
  sIdx, sctx, scdaSize: string;
  stage5SctxOK, stage5ScdaOK, achrOK, acreOK, wrldOK, cellOK, markerOK: boolean;
begin
  Result := 0;
  g_path := OpenOutput('verify.jsonl');

  ours := FindFileByName('StormReproAltStart.esp');
  if not Assigned(ours) then begin
    Emit('FATAL: StormReproAltStart.esp not loaded');
    WriteSummary('{"ok":false,"reason":"esp_not_loaded"}');
    Exit;
  end;
  Emit('Loaded: ' + GetFileName(ours));
  Emit('Records: ' + IntToStr(RecordCount(ours)));

  wrldOK := false; cellOK := false; markerOK := false;
  achrOK := false; acreOK := false;
  stage5SctxOK := false; stage5ScdaOK := false;

  for i := 0 to RecordCount(ours) - 1 do begin
    rec := RecordByIndex(ours, i);
    Emit('  - ' + Signature(rec) + ' ' +
         IntToHex(GetLoadOrderFormID(rec), 8) +
         ' EDID=' + EditorID(rec));
    if (Signature(rec) = 'WRLD') and
       (EditorID(rec) = 'StormReproWRLD') then wrldOK := true;
    if (Signature(rec) = 'CELL') and
       (EditorID(rec) = 'StormReproCell') then cellOK := true;
    if (Signature(rec) = 'REFR') and
       (EditorID(rec) = 'StormReproSpawnMarker') then markerOK := true;
    if (Signature(rec) = 'ACHR') and
       (GetLoadOrderFormID(rec) = $00070106) then begin
      achrOK := SameText(GetElementEditValues(rec, 'Cell'),
                         'StormReproCell "Storm Repro Cell" [CELL:' +
                         IntToHex(GetLoadOrderFormID(rec) and $FF000000, 1) +
                         '???]');
      (* Looser check: just confirm Cell isn't ImperialDungeon01 *)
      achrOK := Pos('ImperialDungeon', GetElementEditValues(rec, 'Cell')) = 0;
      Emit('    ACHR Cell: ' + GetElementEditValues(rec, 'Cell') +
           ' (rehomed=' + LowerCase(BoolToStr(achrOK, True)) + ')');
    end;
    if (Signature(rec) = 'ACRE') and
       (GetLoadOrderFormID(rec) = $00070107) then begin
      acreOK := Pos('ImperialDungeon', GetElementEditValues(rec, 'Cell')) = 0;
      Emit('    ACRE Cell: ' + GetElementEditValues(rec, 'Cell') +
           ' (rehomed=' + LowerCase(BoolToStr(acreOK, True)) + ')');
    end;
    if (Signature(rec) = 'QUST') and
       (GetLoadOrderFormID(rec) = $0002466E) then begin
      (* Find Stage 5, dump its SCTX + SCDA size *)
      for j := 0 to ElementCount(ElementByName(rec, 'Stages')) - 1 do begin
        stage := ElementByIndex(ElementByName(rec, 'Stages'), j);
        sIdx := GetElementEditValues(stage, 'INDX');
        if sIdx = '5' then begin
          items := ElementByName(stage, 'Log Entries');
          if Assigned(items) and (ElementCount(items) > 0) then begin
            item := ElementByIndex(items, 0);
            schr := ElementByName(item, 'Result Script');
            if Assigned(schr) then begin
              sctx := GetElementEditValues(schr, 'SCTX');
              scdaSize := GetElementEditValues(schr, 'SCHR\CompiledSize');
              stage5SctxOK := Pos('StormReproSpawnMarker', sctx) > 0;
              stage5ScdaOK := (scdaSize <> '0') and (scdaSize <> '');
              Emit('    Stage 5 SCTX: (first 100 chars) ' +
                   Copy(sctx, 1, 100));
              Emit('    Stage 5 CompiledSize: ' + scdaSize);
            end;
          end;
        end;
      end;
    end;
  end;

  Emit('---- summary ----');
  Emit('WRLD created:       ' + LowerCase(BoolToStr(wrldOK, True)));
  Emit('CELL created:       ' + LowerCase(BoolToStr(cellOK, True)));
  Emit('Marker created:     ' + LowerCase(BoolToStr(markerOK, True)));
  Emit('ACHR rehomed:       ' + LowerCase(BoolToStr(achrOK, True)));
  Emit('ACRE rehomed:       ' + LowerCase(BoolToStr(acreOK, True)));
  Emit('Stage 5 SCTX OK:    ' + LowerCase(BoolToStr(stage5SctxOK, True)));
  Emit('Stage 5 SCDA built: ' + LowerCase(BoolToStr(stage5ScdaOK, True)));

  if wrldOK and cellOK and markerOK and achrOK and acreOK and
     stage5SctxOK and stage5ScdaOK then
    WriteSummary('{"ok":true}')
  else
    WriteSummary('{"ok":false}');
end;

end.
```

- [ ] **Step 2: Write run_verify.py**

Create `D:/Modlists/Reborn/research/storm_repro/run_verify.py`:

```python
"""Verify StormReproAltStart.esp via xEdit REPL.

Loads the ESP from Data (where Task 5 put it) into a REPL session
alongside Reborn-Minimal, then runs verify_esp.pas.

If the ESP isn't yet in Reborn-Minimal's plugins.txt, this verification
session won't see it. Workaround: temporarily append to plugins.txt
before this verify, OR run in a profile that already has it.
"""
import sys, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))
from repl_client import REPLSession

XEDIT = r"D:\Modlists\Reborn\mods\TES4Edit\TES4Edit 4.1.5f\TES4Edit_patched.exe"
OUT = Path(__file__).parent / "verify.txt"

# To verify, our ESP must be in plugins.txt. Append temporarily.
PLUGINS_TXT = Path(r"D:/Modlists/Reborn/profiles/Reborn-Minimal/plugins.txt")
ESP_BASENAME = "StormReproAltStart.esp"
original = PLUGINS_TXT.read_text(encoding="utf-8")
if ESP_BASENAME not in original:
    PLUGINS_TXT.write_text(original.rstrip() + "\n" + ESP_BASENAME + "\n",
                           encoding="utf-8")
    print(f"temporarily added {ESP_BASENAME} to plugins.txt")
    restore = True
else:
    restore = False

try:
    with REPLSession(profile="Reborn-Minimal", xedit_exe=XEDIT) as repl:
        r = repl.exec_path(Path(__file__).parent / "verify_esp.pas")

    print(f"envelope_ok={r.envelope.get('ok')}")
    err = r.envelope.get("error")
    if err: print(f"error={err!r}")
    print(f"summary={r.envelope.get('summary')!r}")

    summary = r.envelope.get("summary")
    src_path = None
    if isinstance(summary, dict) and "out" in summary:
        src_path = Path(summary["out"])
    if src_path and src_path.exists():
        lines = []
        for raw in src_path.read_text(encoding="utf-8", errors="replace").splitlines():
            try: lines.append(json.loads(raw))
            except json.JSONDecodeError: lines.append(raw)
        OUT.write_text("\n".join(lines), encoding="utf-8")
        print(f"wrote {OUT} ({len(lines)} lines)")
        print()
        # Print summary section
        for line in lines[-12:]:
            print(f"  {line}")
finally:
    if restore:
        PLUGINS_TXT.write_text(original, encoding="utf-8")
        print(f"restored plugins.txt")
```

- [ ] **Step 3: Run verify**

```bash
cd "D:/Modlists/Reborn/research/storm_repro" && python run_verify.py
```

Expected output (last lines):
```
WRLD created:       true
CELL created:       true
Marker created:     true
ACHR rehomed:       true
ACRE rehomed:       true
Stage 5 SCTX OK:    true
Stage 5 SCDA built: true
```

If any line shows `false`: STOP. Diagnose:
- `WRLD/CELL/Marker false` → Task 3 build failure; re-run with diagnostics
- `ACHR/ACRE false` → cell rehome didn't take; check `SetElementNativeValues(achrOvr, 'Cell', ...)` line
- `Stage 5 SCTX OK false` → our SCTX text didn't replace vanilla; check Task 3 stage-finding code
- `Stage 5 SCDA built false` → OBC didn't recompile; revisit Task 5

---

### Task 7: Install as MO2 mod + activate in Reborn-Minimal

- [ ] **Step 1: Create the mod folder**

```bash
mkdir -p "D:/Modlists/Reborn/mods/StormReproAltStart"
```

- [ ] **Step 2: Copy ESP into mod folder**

```bash
cp "D:/Modlists/Reborn/research/storm_repro/StormReproAltStart.esp" \
   "D:/Modlists/Reborn/mods/StormReproAltStart/StormReproAltStart.esp"
```

- [ ] **Step 3: Write meta.ini for the mod**

Create `D:/Modlists/Reborn/mods/StormReproAltStart/meta.ini`:

```ini
[General]
modid=0
version=0.1.0
installationFile=
repository=

[installedFiles]
size=0
```

- [ ] **Step 4: Add to Reborn-Minimal modlist.txt**

The mod must appear in the profile's `modlist.txt` to be considered installed. MO2 modlist.txt is bottom-up priority order. To make it overlay everything else, prefix with `+` and put it AT THE TOP of the file (which is the last-loaded position in MO2's file ordering, per memory `feedback_mo2_left_pane_priority.md`).

```bash
MODLIST="D:/Modlists/Reborn/profiles/Reborn-Minimal/modlist.txt"
# Prepend our line
{ echo "+StormReproAltStart"; cat "$MODLIST"; } > "$MODLIST.tmp" && mv "$MODLIST.tmp" "$MODLIST"
```

- [ ] **Step 5: Add to plugins.txt at end (last-loaded = winning override)**

```bash
PLUGINS="D:/Modlists/Reborn/profiles/Reborn-Minimal/plugins.txt"
grep -q "^StormReproAltStart.esp" "$PLUGINS" || \
  echo "StormReproAltStart.esp" >> "$PLUGINS"
cat "$PLUGINS" | tail -5
```

Expected: `StormReproAltStart.esp` appears as the last line of plugins.txt.

- [ ] **Step 6: Verify load order is correct in xEdit (one-time sanity check)**

```bash
cd "D:/Modlists/Reborn/research/storm_repro" && python run_verify.py
```

(Yes, run again — now we're verifying with the real plugins.txt entry, not the temporary one. Expected: same all-true output.)

---

### Task 8: Test repro

The moment of truth: fresh new game must land in the test cell with StormLog capturing a storm event.

- [ ] **Step 1: Confirm StormLog is active in Reborn-Minimal**

```bash
grep "^StormLog$\|^+StormLog" "D:/Modlists/Reborn/profiles/Reborn-Minimal/modlist.txt"
ls "D:/Modlists/Reborn/mods/StormLog/Data/OBSE/Plugins/StormLog.dll"
```

If StormLog isn't enabled, enable it (`+StormLog` line in modlist.txt) before continuing.

- [ ] **Step 2: Clear StormLog's CSV output directory (for clean test)**

```bash
rm -f "D:/Modlists/Reborn/Stock Game/Data/OBSE/Plugins/StormLog/"*.csv 2>&1 || true
```

- [ ] **Step 3: Launch Oblivion through MO2 in Reborn-Minimal**

Manual step — the user clicks "Run" in MO2 with Reborn-Minimal selected. Expected sequence:

1. Main menu loads
2. Click "New Game"
3. ImperialDungeon01 briefly visible (< 1 second)
4. (Possible) Race menu blip — CloseAllMenus should kill it; if it persists, click any race
5. Player drops into StormReproCell facing VirtueRider on horseback

If the player ends up in vanilla ImperialDungeon01 (still in prison): our Stage 5 override didn't fire. Most likely cause: load order wrong (PSMQD winning). Verify in `xEdit` that our override is at the END of Charactergen's override chain.

- [ ] **Step 4: Verify storm captured**

Within ~10 seconds of arriving in StormReproCell, the FaceGen pipeline should fire on the VirtueRider ACHR. Check StormLog's CSV:

```bash
ls -la "D:/Modlists/Reborn/Stock Game/Data/OBSE/Plugins/StormLog/" 2>&1 | head -5
```

Open the most recent CSV. Expected: rows with `npc_formid=000700CC` (the NPC_ template `ImperialLegionRiderVirtue` referenced by ACHR `00070106`) and event type `STORM` or at least `BURST_END` with elevated `calls_in_window`.

If no rows at all: cell-load FaceGen path differs from our assumption. Open question — consult with StormLog L1 hook logging.

If rows present but no STORM: the burst threshold (default `iBurstThresholdPerSecond=5`) wasn't crossed in 1s. Lower threshold or accept BURST_END as repro evidence. Either way the repro is producing observable FaceGen-pipeline activity.

- [ ] **Step 5: Cycle test**

Quit to main menu. Click "New Game" again. Same outcome expected. Confirms the repro is reusable, not save-state dependent.

- [ ] **Step 6: Document the result**

Append a results section to the spec or write a separate observations note: cycle time per repro (seconds from New-Game click to player-in-cell), whether race menu blip is suppressed, what StormLog captured.

---

## Self-Review

**Spec coverage check:**
- Goal 1 (isolated cell, exact vanilla NPC, < few seconds): Tasks 1–4 author ESP, Task 8 tests timing. ✓
- Goal 2 (no chargen UI): Task 2's SCTX includes `CloseAllMenus`; Task 8 step 3 verifies behavior. ✓
- Goal 3 (exact vanilla ACHR `00070106` + ACRE `00070107` — not clone): Task 3 step 1 uses `wbCopyElementToFile` on those exact FormIDs. ✓
- Goal 4 (contained, toggleable per-profile): Task 7 installs as a standalone MO2 mod with isolated meta.ini. ✓
- Risks (race menu uncertain, cell-load timing, AI packages broken): documented in spec; Task 8 step 3/4 catches each. ✓

**Placeholder scan:** No "TBD" / "implement later" / "similar to Task N" / vague-validation steps. Each step has explicit code, exact paths, exact commands.

**Type consistency:** ESP basename `StormReproAltStart.esp` used uniformly. CELL EDID `StormReproCell`. Marker EDID `StormReproSpawnMarker`. WRLD EDID `StormReproWRLD`. SCTX path `D:/Modlists/Reborn/research/storm_repro/stage5.sctx` used in both build_esp.pas and obc_run. FormIDs `00070106` / `00070107` / `0002466E` consistent throughout.

**One known soft spot:** Task 5 Step 2 assumes OBC env-var names `OBC_TARGET_PLUGIN` / `OBC_TARGET_FORMID` / `OBC_LOG_PATH`. These names should be verified against `D:/Modlists/_clones/oblivion-batch-compile/README.md` at the top of Task 5 before invoking. The task includes a step (5.1) to read the README; the worker must update the names in `run_obc.py` if the actual contract differs.

---

## Decision log additions (beyond the spec's)

- **Why xEdit REPL not CSE GUI:** OBC requires CSE to compile SCDA either way. Using xEdit REPL to author records means the only manual CSE step is invoked headless by OBC. Net: zero GUI clicks, fully reproducible.
- **Why a probe task first:** Authoring brand-new WRLD records via REPL is not in the existing memory inventory. `feedback_test_before_design.md` warns against designing on untested primitives.
- **Why install as a top-of-modlist mod:** Per `feedback_mo2_left_pane_priority.md`, top of modlist.txt = highest priority in MO2's USVFS overlay. Our ESP override of Charactergen must outrank PSMQD's override; load-order-wise (plugins.txt) we append to end. Mod-list-wise (modlist.txt) we prepend to top. Both produce "wins."
- **Why we accept the race-menu blip rather than skipping its trigger:** The engine's race-menu trigger timing is undocumented. `CloseAllMenus` is the safety net. Worst case = one click per repro cycle, which is acceptable for the test workflow.
