---
id: 32
title: "With fps60 on, Crash 1's in-betweens repeat the previous picture"
status: open
symptom: the 60 fps option doubles every present but moves nothing between them; motion looks exactly as at 30 fps
state_items: S005,S007,S017
tags: crash1,crash2,crash3,fps60,interpolation
created: 2026-10-28
updated: 2026-11-15
---

Reproduce: `crash_port` headless, `pick crash1`, idle into the Jungle Rollers attract demo, `pause`, `state save`; then
two runs that `state load`, `step 150` and `preseq` (90 presents at `fps60=0`, 180 at `fps60=1`). Every odd
present of the `fps60=1` run is pixel-identical to the real frame before it (mean squared error 0.0 at 256x120 grey
over all 90 in-betweens), while the playcheck line counts them as `in_betweens=241`.

Cause: no Crash title publishes producers or draw-site tags, so the Record presenter has nothing to blend and draws
the held record again. `titles/crash1/entry/runtime.h` and `titles/crash2/entry/runtime.h` still declare
`interpolates()`, which offers the player a 60 fps setting that only adds a field of latency. Crash 3 declares nothing.

Proper fix: per title, tag the world, object and camera draw sites (or native producers) so each in-between is
built from previous/current simulation state (psxport F-FPS60.1); until then `interpolates()` should not offer the
setting. No interpolation capture goes in the README until this is done.

Crash 2 (2026-11-13): fixed by replaying its draw section (`titles/crash2/frame/`, crash2-decomp
`docs/notes/frame-loop.md`); `warp 15` (scene 0x15) held up, the `fps60dump` in-betweens land halfway (S007). Crash 1 still
has no section, and Crash 3 no interpolation. Crash 2's remaining gaps: the retail morph drawer adds two frames at a fixed
equal weight, so only a t = 1/2 pose whose frames pair up is drawn halfway and the rest step at 30 Hz; the prepared-cut and
zone-walk cut declarations (`titles/crash2/frame/camera_cuts.cpp`) have not been seen firing in a run.

Crash 1 (2026-11-15): replays `CORE_Loop`'s picture half (`titles/crash1/frame/`): `Gpu_Update`'s end state for the
render buffer, then `Gfx_UpdateMatrices` to `Gpu_Update`'s `Audio_Update` with object logic off, sampled at
`Gool_UpdateObjects`' return. The camera and the objects' placement blend; in the attract demo in-betweens land halfway.
Poses: `Gfx_TransformSvtx` draws a blend of the object's last real SVTX frame and the replay's, written over the frame
for that call (`crash1::PoseBlend`); a vertex rounds to a whole frame unit. A replay runs after the newest tick's
object pass and before its `Gpu_Update`: the screen shake it would read is the tick after's, so the newest tick's is
written back, and the fade it steps to is the newest tick's, drawn t of the way from the tick before's
(`crash1::ScreenEffects`). Open: CVTX models (`Gfx_TransformCvtx`)
draw the later frame; no camera cut but the scene's is declared; the walk-order question of Crash 2's issue 0038 has not been checked here.

Crash 3 (2026-11-15): replays the same picture half through the shared `crash::DrawReplay` (`titles/crash3/entry/facts.cpp`
`kReplay`, crash3-decomp `docs/notes/frame-loop.md`). A t = 1 replay matched the real frame in all but the first frame
after each `Sys_LoadCheckpoint` 0x80030388, which 0x800307F4 and the GOOL op 0x1C handler call without a scene request; that restore
is now a declared cut (`titles/crash3/frame/camera_cuts.cpp`). Placement and the camera land halfway; open: Crash's
vertex-animated pose is drawn as the later tick (its pose fetch and morph drawer are not read), and `Cam_Update`'s own
cuts are not mapped.
