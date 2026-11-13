---
id: 38
title: "Crash 2: the t = 1 replay differs from the real chain in runs of three frames"
status: open
symptom: a replay with nothing blended links a packet chain that is not byte-identical to the real draw's in 13 of 299 frames
state_items: S007
tags: crash2,fps60,interpolation
created: 2026-11-13
updated: 2026-11-13
---

Reproduce: `warp 15` (scene 0x15) with up held (`PSXPORT_FORCE_HOLD=FFEF`), `state save` 3 s after the warp; load it
unpaced at fps60 for 300 frames with a temporary probe at `Sys_BeforeDrawObjects` that runs `DrawSection` once more at
t = 1 and compares its captured chain (packets and words) with the real chain at `Snd_Update`. 286 frames match and
13 differ, in runs of three consecutive frames (packet counts close to the real ones). The same run with the retail
morph readers (no `crash2::MorphReader`) differs in the same 13 frames, so the readers are not the cause. Scene 0x6
from the warp matches in 299 of 299.

Not yet traced: which packets differ, and which of the section's inputs the real frame's logic pass (`Obj_UpdateAll`
with `g_FrameFlags` 0x8 set) leaves different from the replay's logic-off pass.
