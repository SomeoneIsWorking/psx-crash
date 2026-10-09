---
id: 28
title: "Crash 3's CoreLoop calls GpuUpdate once at the loop end with no stub after it, so the Crash 2 frame boundary does not transfer"
status: open
symptom: Crash 3 still refuses to boot; no native_boot or native_frame facts exist
state_items: S008, S010
tags: crash3,frame-boundary,vsync
created: 2026-10-09
updated: 2026-10-09
---

Measured: CoreLoop 0x8001166C (loop top 0x800116D8), GpuUpdate FUN_80016634(0) called at 0x80011EFC,
done flag 0x800608E4, scene request word 0x80060AC0, scene loader FUN_80014BC8, VSync leaf
0x8004B2A8..0x8004B3F0. The only stubs are FUN_80011FB8 (called at 0x80011868) and FUN_80011FC0
(called at 0x80011D90), both mid-iteration. libcd bodies match Crash 2 exactly (cd_initialize
0x80047188, cd_control 0x80045A1C, cd_sync 0x80046574, cd_read 0x80047E40).

Next: take the boundary at the VSync jal returns inside FUN_80016634 (jal sites near 0x80016AA8 and
0x80016AD4) with the resume point at the loop top, then find callback_initialize, gpu_watchdog_check,
the pad buffers and the gp-relative globals to write crash3 facts.
