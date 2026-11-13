---
id: 40
title: "Crash 2: the warp-in white fade steps with the later tick"
status: resolved
symptom: at a level's warp-in the screen's white fade changes only on real presents; each in-between already shows the next tick's fade
state_items: S007
tags: crash2,interpolation,fade
created: 2026-11-14
updated: 2026-11-14
---

Reproduce: `warp 2` (scene 0x6), state saved 0.3 s after the warp (`scratch/morph/run/s6_warp2.state`), fps60 on,
ires 0, sink 3840x2160, unpaced, `PSXPORT_DEBUG=fps60dump`. Mean difference of each in-between to its previous and
next real present (480x270 grey): presents 4 to 10 are 19-32 from the previous and 3-7 from the next, where scene
0x15's run is about 12.5 to each. From present 11 the in-betweens are balanced again. Crop:
`scratch/morph/nolit/s6_warpin.png` (real 4, in-between, real 5): the in-between's whiteness is the later real's.

Expected: the fade level at t = 1/2 halfway between the two ticks', as the camera and objects are.

Cause: `titles/crash2/frame/draw_section.cpp` `DrawSection::draw` runs `Gfx_EndFrame` to `Snd_Update`, which steps the
fade word 0x8006CD18 by 0x8006CD1C before `Gfx_DrawFade` draws it, and nothing blended the fade, so each replay drew
the later tick's level (crash2-decomp `docs/notes/frame-loop.md`, Fade). After `warp 2` the word enters at 0x100 and
falls 0x20 a tick; the real frames draw 0xE0 down to 0.

Fix (2026-11-14): `crash2::FadeLevel` (`titles/crash2/frame/fade_level.*`), a `DrawReplay` motion source with an
override at `Gfx_DrawFade`. A real call records its level, its quad's grey and the blend mode. In a replay the quad
takes the grey t of the way from the earlier tick's to the later's. A tick that drew none counts as grey 0, and when
the later tick draws none, the earlier tick's quad is linked to fade it out. Two ticks in different blend modes draw
the later one, and the run-end report counts those as unpaired.

Proof, same state and input, `scratch/fade/` (`fade_run.py`, `halfway.py`):

- Mean brightness of real 4, the in-between and real 5 onward (`s6_warpin_fix.png`, HEAD `s6_warpin_head.png`): 206.5,
  195.6, 177.2, 170.3, 156.8, 142.8, 128.6. On HEAD each in-between equalled the later present (177.0, 156.7, 128.6).
- Whole-frame difference of in-betweens 4-10 to the previous and next real present: 9.9/9.7, 11.8/18.9, 8.6/14.4,
  16.9/15.2, 15.7/10.3, 17.9/16.0, 16.3/15.9, against 19-32/3-7 before. The near-white frames are uneven because the
  additive white saturates.
- 17 of 17 real presents are identical to HEAD's. With every replay forced to t = 1 (a temporary probe in psxport
  `ReplayFrame::beforeDraw`), 16 of 16 in-betweens are pixel-identical to the next real present.
- Run end: `fade: 0 of 7 item draw(s) unpaired`.
