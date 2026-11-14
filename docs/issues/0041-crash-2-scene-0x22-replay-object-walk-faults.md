---
id: 41
title: "Crash 2: a replay's object walk faults in scene 0x22 after a checkpoint restore"
status: resolved
symptom: the product aborts in scene 0x22 shortly after a spawn; a replayed Obj_Update faults in guest code
state_items: S007
tags: crash2,fps60,interpolation,crash
created: 2026-11-14
updated: 2026-11-14
---
Reproduce: `scratch/cuts/phase22.py` from a paused state saved in scene 0x22 after spawn 1, reached through scene
0x15 (`warp scene 0x15`, 4 s, `warp scene 0x22`, 4 s, `spawn 1`, 3 s); `spawn 2` then `step 200` aborted every run:
a replayed `Obj_Update` exited as a Lightrec memory fault at 0x80042FD0 (exit 139). The same state reached without
scene 0x15 never faulted, because which NSF pages are resident decides it.

Cause: `Obj_Update` with logic off places an object through `func_8001AA80` alone, and for a vertex animation that
passes `*(block+0x28)+0x10` to the scale step (0x8004400C via table 0x8005B650). Block +0x28 is the model entry
`func_8001A4A4` opened at the object's last logic update. The retail callers of `func_8001AA80` on the logic paths
(the GOOL interpreter and `func_8001D63C`) run `func_8001A4A4` just before it. In the real walk a later object's
`Nsf_OpenEntry` paged out the faulting object's model entry (0x801A4BD8, word +0x10 0x801A4C04 became 0x38AB3895), so
the replay's placement read garbage as a pointer.

Fix: `crash2::ObjectWalk` runs `func_8001A4A4` for each vertex object (`crash2::pose_blend::fetchPose`) before its
replayed `Obj_Update`, as the logic paths do. Proof: the same state with D = 0..6 runs 200 frames each with no executor
error; the in-betweens through spawn 2 draw the scene intact, the fade-in halfway (mean grey 1.02, 2.35, 4.09, 6.72,
8.38) (`scratch/cuts/s22_spawn2_a.png`, `s22_spawn2_b.png`).
