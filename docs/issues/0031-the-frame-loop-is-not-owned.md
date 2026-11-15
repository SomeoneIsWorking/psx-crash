# 0031 — The frame loop is not owned

Status: open (2026-10-28). Owner: `game/frame/frame_driver.cpp:FrameDriver::stepFrame`.

## What the code does
Each step runs the core loop iteration (Crash 1 `0x80012030`, Crash 2 `0x80011848`, Crash 3 `0x800116D8`; `native_frame.iteration` in each `executable.json`) as guest code to GpuUpdate's two VSync waits and the loading swap's wait. Each wait delivers `Timing::vsyncFields(0)` fields on the guest clock (`Timing::advanceDisplayFields`), CNT2 and the VBlank callbacks run there, and the present only plans its deadline (`FramePacer::presentedFieldsDeadline`).

## Why it matters
The workspace rule (psxport `docs/workspace/WORKSPACE.md`, "Each title owns its frame loop") wants the frame driver to run the guest's per-frame update and draw and decide the cadence, replacing the guest's VSync waits. Until then the guest's waits still decide the cadence.

## What blocks it
GpuUpdate (`native_frame` `gpu_update` range) is guest code whose two VSync calls are the cadence; owning the loop means a native GpuUpdate that swaps and returns, with the driver delivering the iteration's fields. The iteration body hashes in `executable.json` pin the three bodies to port.
The waits already count `Timing::vsyncFields`, so an overrunning frame keeps its VBlanks, audio and fields on the guest clock's grid; the native GpuUpdate keeps that count at the wait it replaces.

## Crash 3's presents alternated 1, 1, 2 fields (2026-11-15, fixed)
In a level (`warp 5`, scene 0x07, unpaced, `presentseq`), Crash 3 commits 1, 1, 2 fields per frame: 2,399 presents over
3,209 fields. Retail GpuUpdate (0x80016634, the waits at 0x80016A8C) waits once while the VBlank counter `gp+0x70` is
fewer than 2 past the shown frame's stamp (`+0x1C`), and again while CNT2's tick word `0x8003AE8C` is fewer than 0x19
past the shown frame's (`+0x10`); it then stamps the new frame with both. Its logic takes about 0.8 field of guest time,
so on a console the first wait returns at the next VBlank, the second follows, and every frame is 2 fields (30 fps). Here
the VBlank interrupt, whose callback 0x80017AD0 bumps `gp+0x70`, is raised only at the frame's commit, after GpuUpdate
stamped the frame; the next frame then finds the counter already 2 past the stamp and skips both waits
(`FrameProgram::limiter`), and the one after waits once. Fixed by delivering each wait's fields and VBlanks at the
wait: in `warp 5` 1,497 of the last 1,500 presents cover 2 fields; Crash 1's attract demo and Crash 2's `warp 15` stay at
2, and a paced Crash 2 run advances 0.5 tick on 5,985 of 6,012 vblanks.
