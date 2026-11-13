---
id: 38
title: "Crash 2: the t = 1 replay differs from the real chain in runs of three frames"
status: resolved
symptom: a replay with nothing blended links a packet chain that is not byte-identical to the real draw's in 13 of 299 frames
state_items: S007
tags: crash2,fps60,interpolation
created: 2026-11-13
updated: 2026-11-13
---

Reproduce: `warp 15` (scene 0x15) with up held (`PSXPORT_FORCE_HOLD=FFEF`), `state save` 3 s after the warp; load it
unpaced at fps60 for 300 frames with a temporary probe at `Sys_BeforeDrawObjects` that runs `DrawSection` once more at
t = 1 and compares its captured chain (packets and words) with the real chain at `Snd_Update`, and logs the objects
each pass hands `Gfx_QueueObject`. 286 frames matched and 13 differed, in runs of three consecutive frames; the retail
morph readers differed in the same frames.

Cause: `DrawSection` ran `Obj_UpdateAll` on the lists the logic pass left, but the real frame queues its objects in
the order its walk met them while the logic changed the lists under it (crash2-decomp `docs/notes/frame-loop.md`,
Objects). In the runs of three, objects spawned that tick were queued by the real walk where it reached them and by the
replay at the head of their list, so every packet behind them moved and equal-depth primitives were inserted in another
order. After the checkpoint restore an object queued twice in the real walk was queued once by the replay, and objects
that died after their visit were still queued by it.

Fix: `crash2::ObjectWalk` records the objects the real walk hands `Gfx_QueueObject` (its one caller is `Obj_Update`'s
tail) and, inside a replay, `Obj_ForEachList` called with `Obj_Update` runs `Obj_Update` with logic off on those
objects in that order instead of walking the lists.

Proof: the same probe matches in 298 of 299 frames of scene 0x15; the one left is the declared checkpoint cut, whose
interval is held. Scene 0x6 matches in 299 of 299. In-betweens of Crash's run at logic frames 101-103, where objects
spawn, stay halfway between their neighbours (`scratch/morph/dust_walk_101_103.png`: whole-crop difference
9.68/9.47, 9.88/9.05, 10.04/10.07 to the previous and next real present).
