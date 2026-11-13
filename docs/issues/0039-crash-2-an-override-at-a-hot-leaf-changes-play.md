---
id: 39
title: "Crash 2: an override at the morph readers' entries changes play from frame 64"
status: resolved
symptom: with the same state, input and guest clock, Crash's position differs from frame 64 once overrides sit at the morph readers
state_items: S007
tags: crash2,determinism,overrides
created: 2026-11-13
updated: 2026-11-14
---

Reproduce: `scratch/morph/run/s15_run.state` (scene 0x15, up held with `PSXPORT_FORCE_HOLD=FFEF`), unpaced, fps60
off, 300 frames, with a temporary line at the end of `FrameDriver::stepFrame` logging the guest clock and
`g_Player`'s position (+0x60..+0x68). Without `MorphReader::registerOverrides` and with it, the guest clock at every
frame end is the same and the position is the same through frame 63; at frame 64 x and z differ (10,162,176 against
10,164,224). Overrides at 0x8003ED14 and 0x80040D14 that only call the retail readers part at the same frame with the
same values as the native readers, so the readers' results are not the cause: the override boundary is.

Cause: psxport `runtime/cpu/lightrec_executor.cpp` `Impl::commitDeviceClock` committed guest time at device
accesses from Lightrec's cycle counter, which runs about 1% ahead of twice the executed instructions, while the
segment end charged instructions and saturated at what was already committed. A segment's charge was therefore
the larger of the two, and the override boundary split frame 61's main-loop segment (start 0x80011D94, 172,930
instructions, 1,819 ticks over) into short ones (55 over). The 1,764-tick gap moved CNT2 by one count, the time
counter at 0x80063468 (and its copy at 0x8003BEA4) read 0x5122 instead of 0x5123 at frame 62, and play parted at 64.

Fix: psxport d4de29d4 `fix(cpu): device clock commits count instructions, not Lightrec cycles`; the ledger
(`runtime/cpu/segment_clock.*`) commits Lightrec's executed-instruction counter, so every segment charges exactly
its instructions. Shown: per-frame hashes of RAM, scratchpad and the guest clock are equal between the build
without `MorphReader::registerOverrides`, the pass-through overrides (63 frames) and the native readers (600
frames). Tomba 2 (3,000 frames through the FMV and title) and Spyro 1 (3,000 steps, demo in level) run without
faults on it.
