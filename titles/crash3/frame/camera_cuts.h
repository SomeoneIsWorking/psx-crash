// camera_cuts.h — Crash 3's own camera cuts, declared to crash::FrameCut: the checkpoint load's camera restore.
#pragma once

class Core;

namespace crash3::camera_cuts {

void registerOverrides(Core &core);

} // namespace crash3::camera_cuts
