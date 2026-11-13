---
id: 40
title: "Crash 2: the warp-in white fade steps with the later tick"
status: open
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

Not yet traced: which word holds the fade level, which draw reads it, and whether `crash2::DrawReplay` blends it or
replays the later tick's value.
