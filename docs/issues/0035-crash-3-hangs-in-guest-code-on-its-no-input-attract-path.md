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

Not yet traced: which guest loop spins and what it waits for. Next step: `regs` / a store watch at the hang, then
decompile the looping function.

Effect on the selector: Crash 3's attract demo is not shown; its panel ends the product when focused long enough.
