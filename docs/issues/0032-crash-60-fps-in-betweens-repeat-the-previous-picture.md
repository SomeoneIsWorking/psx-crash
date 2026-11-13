---
id: 32
title: "With fps60 on, Crash 1's in-betweens repeat the previous picture"
status: open
symptom: the 60 fps option doubles every present but moves nothing between them; motion looks exactly as at 30 fps
state_items: S005,S007,S017
tags: crash1,crash2,crash3,fps60,interpolation
created: 2026-10-28
updated: 2026-11-13
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
`docs/notes/frame-loop.md`); `warp 15` (scene 0x15) in-betweens land halfway (S007). Crash 1 still has no section, and Crash 3 no
interpolation. Crash 2's remaining gaps: animation frames and scale step at 30 Hz, objects with state flag 0x100000 run
their GOOL code in the replay's `Obj_UpdateAll(0)`, a second `Gfx_UpdateView` steps the shake once more, and in-level
camera cuts are not declared.
