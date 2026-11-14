---
id: 42
title: "Crash 2: the real frame faults at 0x80014B74 after a scene 0x1D spawn warp with up held"
status: open
symptom: the product aborts (exit 139) right after `spawn 6` in scene 0x1D while up is held
state_items: S007
tags: crash2,crash,warp
created: 2026-11-14
updated: 2026-11-14
---

Seen once: unpaced, fps60, ires 0, `M_HOLD=FFEF M_DWELL=3 scratch/cuts/sweep.py sweepup.log` (every scene and spawn,
`hold FFEF` after each warp). After `warp` to scene 0x1D spawn 6 (the store watch saw `g_CameraMode` = 5 at
0x80014DE8, frame 46726) the logic frame 46875 left guest execution as a Lightrec fault at 0x80014B74, 4,252,319 cycles
in, and the product exited 139. This is the real frame (`crash-frame:error`), not a replay. Two no-input sweeps
passed the same warp.

Not traced: no saved state before the fault yet. Next: a state saved in scene 0x1D just before `spawn 6` with up
held, stepped a frame at a time to the fault, then the faulting function decompiled.
