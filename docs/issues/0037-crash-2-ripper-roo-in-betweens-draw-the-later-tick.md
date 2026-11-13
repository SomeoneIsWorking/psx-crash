---
id: 37
title: "Crash 2: Ripper Roo's in-betweens draw him at the later tick"
status: open
symptom: at 60 fps Ripper Roo's intro descent moves at 30 Hz while the camera and Crash move at 60 Hz
state_items: S007
tags: crash2,fps60,interpolation
created: 2026-11-13
updated: 2026-11-13
---

Reproduce: `warp 2` (scene 0x6, Ripper Roo), wait 8 s, `state save`; load it unpaced at ires 0, fps60,
`PSXPORT_DEBUG=fps60dump,presentseq`, no input. Every interval is replayed at t = 1/2 (`composed=1`), but the in-between's
picture differs from the next real present only around Roo (box 2589,96-3063,417 at 5472x1920), and there by
little: his body in each in-between stands where the later tick puts it (`scratch/interp/shots/scene6_roo_64_67.png`).
The warp-in rings around Crash also land on the later tick.

Cause: `titles/crash2/frame/pose_blend.cpp` blended the pose inside `func_8001A90C`, which only an object's logic calls
(`func_8001A4A4`); with `g_FrameFlags` 0x8 clear the replay never reached it, so every logic-driven object kept the later
tick's frames. Roo (pool object 0, executable 0x8019D2E4) descends by his animation, so he drew at the later pose with a
halfway position. Separately, the player's record (`g_Player`) is not in the pool and was not tracked: Crash was drawn at
the later tick's position in every in-between.

Fixed (2026-11-13): the pose is refetched for both ticks at `Gfx_QueueObject`, and the player has his own track. Scene 0x6:
Roo's in-between crop differs from the earlier real present by 8.85 and the later one by 5.52 (10.62 between the two),
where before it was 10.39 and 2.08 (`scratch/interp/shots/scene6_roo_fixed.png`); player 0 of 75 unpaired.

Scenes 0x8 (Komodo Bros) and 0x9: in-betweens sit between their neighbours (scene 0x9 whole frame 2.13 against 2.15, the
Komodo brother 8.4 against 8.7); a hit's white flash is a per-tick colour and steps with the later tick.

Remaining: the warp-in rings (executable 0x801BD2E0) play a half-rate animation (header bit 0). Between frame 6 (stored
frame 3 twice) and 7 (3 and 4) the halfway pose is a quarter of the way, which the equal-weight morph drawer cannot draw,
so they step. Drawing them needs a weighted vertex reader in place of `func_8003ED14`.
