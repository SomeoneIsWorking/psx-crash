---
id: 28
title: "Crash 3's CoreLoop calls GpuUpdate once at the loop end with no stub after it, so the Crash 2 frame boundary does not transfer"
status: resolved
symptom: Crash 3 still refuses to boot; no native_boot or native_frame facts exist
state_items: S008, S010
tags: crash3,frame-boundary,vsync
created: 2026-10-09
updated: 2026-10-15
---

Measured: CoreLoop 0x8001166C (loop top 0x800116D8), GpuUpdate FUN_80016634(0) called at 0x80011EFC,
done flag 0x800608E4, scene request word 0x80060AC0, scene loader FUN_80014BC8, VSync leaf
0x8004B2A8..0x8004B3F0. The only stubs are FUN_80011FB8 (called at 0x80011868) and FUN_80011FC0
(called at 0x80011D90), both mid-iteration. libcd bodies match Crash 2 exactly (cd_initialize
0x80047188, cd_control 0x80045A1C, cd_sync 0x80046574, cd_read 0x80047E40).

Next: take the boundary at the VSync jal returns inside FUN_80016634 (jal sites near 0x80016AA8 and
0x80016AD4) with the resume point at the loop top, then find callback_initialize, gpu_watchdog_check,
the pad buffers and the gp-relative globals to write crash3 facts.

## Resolution

Crash 3 takes Crash 1's boundary: the transition override sits at the GpuUpdate return 0x80011F04 inside
CoreLoop, and each host frame after the first resumes at 0x800116D8. The VSync waits are the jal sites at
0x80016AA8 and 0x80016AD4. `titles/crash3/executable.json` now carries `native_boot` and `native_frame`;
`titles/crash3/entry/facts.cpp` the rest. Two differences from Crash 2: Init programs CNT2 with target
0x1010, so `crash::FrameDriver` takes the period from `SetRCnt`'s argument; and GpuUpdate's limiter
(0x80016A8C) skips both waits when the frame already spanned two vblanks (the VSync callback 0x80017AD0
counts `gp+0x70`); such a frame commits the fields up to its VBlank (`FrameProgram::limiter`). The GPU watchdog check differs from Crash 1 and 2
and runs as guest code; libpad reads the pad through the title's own SIO code.

Verified headless: boots to the title menu, `warp 5` (N. Gin) and `warp 3` (Dingodile) run a boss level,
recordcheck 1x 4:3 3,058 presents 0 mismatched.
