---
id: 39
title: "Crash 2: an override at the morph readers' entries changes play from frame 64"
status: open
symptom: with the same state, input and guest clock, Crash's position differs from frame 64 once overrides sit at the morph readers
state_items: S007
tags: crash2,determinism,overrides
created: 2026-11-13
updated: 2026-11-13
---

Reproduce: `scratch/morph/run/s15_run.state` (scene 0x15, up held with `PSXPORT_FORCE_HOLD=FFEF`), unpaced, fps60
off, 300 frames, with a temporary line at the end of `FrameDriver::stepFrame` logging the guest clock and
`g_Player`'s position (+0x60..+0x68). Without `MorphReader::registerOverrides` and with it, the guest clock at every
frame end is the same and the position is the same through frame 63; at frame 64 x and z differ (10,162,176 against
10,164,224). Overrides at 0x8003ED14 and 0x80040D14 that only call the retail readers part at the same frame with the
same values as the native readers, so the readers' results are not the cause: the override boundary is.

Not yet traced: which guest-visible event moves (an interrupt taken at another instruction of the frame is the first
thing to check against the executor's segment ends), and whether any title's play depends on it.
