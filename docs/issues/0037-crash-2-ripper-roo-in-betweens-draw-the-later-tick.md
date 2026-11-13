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

The scan found one object in scenes 0x6, 0x8 and 0x9 whose logic F100 (0x100000) forces on, at 0x8009F6A8. That it is
Roo is not yet shown, and neither is where his drawn position comes from. The replay clears `g_FrameFlags` 0x8, so
F100 does not force his logic there (crash2-decomp `docs/notes/frame-loop.md`). His record's 0x60-0x68 are blended
like every pooled object's, so his draw must read a position the tracks do not hold.

Next: decompile what `Gfx_QueueObject` reads for an object with F100 set, then track that state.
