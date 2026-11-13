// camera_cuts.h — Crash 2's own camera cuts, declared to crash::FrameCut: Cam_Update's mode 7 handler taking a prepared
// cut to another zone's path, its mode 9 handler walking to a zone's path, and Sys_LoadCheckpoint's camera restore.
#pragma once

class Core;

namespace crash2::camera_cuts {

void registerOverrides(Core &core);

} // namespace crash2::camera_cuts
