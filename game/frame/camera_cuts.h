// camera_cuts.h — a Crash 2 or Crash 3 camera that jumps, declared to crash::FrameCut: Cam_Update's prepared cut to
// another zone's path once its go byte is set, its zone walk, and Sys_LoadCheckpoint's camera restore (crash2-decomp
// docs/notes/camera-cuts.md, crash3-decomp docs/notes/frame-loop.md, Camera modes).
#pragma once

#include <cstdint>

class Core;

namespace crash {

struct CameraCutFacts {
  // The prepared cut's mode handler and the go byte Cam_Update clears and its messages set.
  std::uint32_t preparedCut;
  std::uint32_t cutGo;
  std::uint32_t zoneWalk;
  std::uint32_t loadCheckpoint;
};

class CameraCuts {
public:
  explicit CameraCuts(const CameraCutFacts &facts) : facts_(facts) {}

  static void registerOverrides(Core &core, const CameraCutFacts &facts);

private:
  static void preparedCutOverride(Core *core);
  static void zoneWalkOverride(Core *core);
  static void loadCheckpointOverride(Core *core);

  CameraCutFacts facts_;
};

} // namespace crash
