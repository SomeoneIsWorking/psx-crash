---
id: 35
title: "Crash 3 hangs in guest code on its no-input attract path"
status: open
symptom: left with no input, Crash 3 boots into a level scene and about 3770 presents later stops presenting; the watchdog finds the host inside JIT code
state_items: S008
tags: crash3,attract,picker
created: 2026-11-03
updated: 2026-11-03
---

Reproduce: `crash_port scratch/bin/crash3/SCUS_942.44` hidden, unpaced, sink 3840x2160, ires 0, fps60 1, no input,
`PSXPORT_NATIVE_FRAMES=6000`, `PSXPORT_WATCHDOG=300` (`scratch/picker2/solo3`). The last present is about 3768
(`[sinkidle] ... presented 3768`); 300 s later the watchdog fires with the host in a translated block
(`crash_port() [0x7bed89]` then a JIT address). The same hang ends the selector walk with Crash 3 focused at step
4053 (`scratch/picker2/walk`, `crash3_00.png` shows the scene it reached: Crash in water on a level path, no
"DEMO" caption), so it is the title's own no-input path, not the selector.

## Cause (traced 2026-11-03, `scratch/picker2b/c3b`, `scratch/picker2b/decomp*`)
The guest is not stuck in one block: Lightrec exits on budget and `current_cycle` restarts each turn, with the
guest inside `FUN_80012eac` (ra `0x80012F08`) at every sample. `FUN_80012eac` is the level loader's wait for
its page loads, called from `FUN_800215e8` (`0x80021B20`) and `FUN_80025070` (`0x8002510C`):

    do { do { ...; FUN_80012cbc(-1);
              if (DAT_800691fc >= 0 && DAT_800608e8 != displayed->0x1C) FUN_80016634(1); }
         while (DAT_80061a58 != 0); } while (DAT_800691fc >= 0);

At the hang `0x80061A58` = 0 (no page load pending) and `0x800691FC` = 4 (the loading screen's frame count,
cleared to -1 only by `FUN_80017204` once it passes 0x3E frames, or `FUN_800154ac`). The loop draws the next
loading frame through `FUN_80016634(1)` only when the VBlank counter `0x800608E8` (gp+0x70, bumped by the VSync
callback `0x80017AD0`) differs from the displayed frame's stamp; both read `0x1537`. The counter never moves
because `crash::FrameDriver` (`game/frame/frame_driver.cpp`, `runGuestToBoundary`) delivers display fields only
at the measured GpuUpdate VSync waits (Crash 3 has no `loadingSwap`); a budget-exhausted turn services CNT2 but
no VBlank, so this poll spins forever. Owner: `crash::FrameDriver`, with Crash 3's `FrameProgram` in
`titles/crash3/entry/facts.cpp`.

## Fix (not landed)
The loading frames are host frames: the frame driver must treat this loader's VBlank-counter poll as a display
wait (a second measured loading boundary beside Crash 1's `DisplaySwap`, ending the host frame at the poll and
delivering one field), so `FUN_80016634(1)` draws one loading frame per field until `0x800691FC` reaches -1.
Each loading frame's GpuUpdate then reaches its own measured waits. Not done in this pass.

Effect on the selector: Crash 3's attract demo is not shown; its panel ends the product when focused long enough.
