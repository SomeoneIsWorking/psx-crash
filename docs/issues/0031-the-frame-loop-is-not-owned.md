# 0031 — The frame loop is not owned

Status: open (2026-10-28). Owner: `game/frame/frame_driver.cpp:FrameDriver::stepFrame`.

## What the code does
Each step runs the core loop iteration (Crash 1 `0x80012030`, Crash 2 `0x80011848`, Crash 3 `0x800116D8`; `native_frame.iteration` in each `executable.json`) as guest code to GpuUpdate's two VSync waits and the loading swap's wait, delivering one field per wait.

## Why it matters
The workspace rule (psxport `docs/workspace/WORKSPACE.md`, "Each title owns its frame loop") wants the frame driver to run the guest's per-frame update and draw and decide the cadence, replacing the guest's VSync waits. Until then the loop is on the fallback; it is not yet on `Timing::vsyncFields` either, since every guest VSync is answered with one field whatever its argument.

## What blocks it
GpuUpdate (`native_frame` `gpu_update` range) is guest code whose two VSync calls are the cadence; owning the loop means a native GpuUpdate that swaps and returns, with the driver delivering the iteration's fields. The iteration body hashes in `executable.json` pin the three bodies to port.
The converted driver counts `Timing::vsyncFields` of the loop's own n at the replaced wait (psxport `docs/presentation.md`, Frame fence and pacing), so an overrunning frame keeps its VBlanks, audio and fields on the guest clock's grid; it is done in that conversion, not as a separate step on the current one-field answer.

## Crash 3's presents alternate 1, 1, 2 fields (2026-11-15)
In a level (`warp 5`, scene 0x07, unpaced, `presentseq`), Crash 3 commits 1, 1, 2 fields per frame: 2,399 presents over
3,209 fields. Retail GpuUpdate (0x80016634, the waits at 0x80016A8C) waits once while the VBlank counter `gp+0x70` is
fewer than 2 past the shown frame's stamp (`+0x1C`), and again while CNT2's tick word `0x8003AE8C` is fewer than 0x19
past the shown frame's (`+0x10`); it then stamps the new frame with both. Its logic takes about 0.8 field of guest time,
so on a console the first wait returns at the next VBlank, the second follows, and every frame is 2 fields (30 fps). Here
the VBlank interrupt, whose callback 0x80017AD0 bumps `gp+0x70`, is raised only at the frame's commit, after GpuUpdate
stamped the frame; the next frame then finds the counter already 2 past the stamp and skips both waits
(`FrameProgram::limiter`), and the one after waits once. The conversion above delivers each wait's VBlank at the wait.
