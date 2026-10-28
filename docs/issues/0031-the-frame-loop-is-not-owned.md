# 0031 — The frame loop is not owned

Status: open (2026-10-28). Owner: `game/frame/frame_driver.cpp:FrameDriver::stepFrame`.

## What the code does
Each step runs the core loop iteration (Crash 1 `0x80012030`, Crash 2 `0x80011848`, Crash 3 `0x800116D8`; `native_frame.iteration` in each `executable.json`) as guest code to GpuUpdate's two VSync waits and the loading swap's wait, delivering one field per wait.

## Why it matters
The workspace rule (psxport `docs/workspace/WORKSPACE.md`, "Each title owns its frame loop") wants the frame driver to run the guest's per-frame update and draw and decide the cadence, replacing the guest's VSync waits. Until then the loop is on the fallback; it is not yet on `Timing::vsyncFields` either, since every guest VSync is answered with one field whatever its argument.

## What blocks it
GpuUpdate (`native_frame` `gpu_update` range) is guest code whose two VSync calls are the cadence; owning the loop means a native GpuUpdate that swaps and returns, with the driver delivering the iteration's fields. The iteration body hashes in `executable.json` pin the three bodies to port.
The converted driver counts `Timing::vsyncFields` of the loop's own n at the replaced wait (psxport `docs/presentation.md`, Frame fence and pacing), so an overrunning frame keeps its VBlanks, audio and fields on the guest clock's grid; it is done in that conversion, not as a separate step on the current one-field answer.
