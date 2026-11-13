#pragma once

#include "frame/frame_program.h"

#include <cstdint>
#include <optional>
#include <string>

class Core;

namespace crash {

// A scene arrival of the warp catalogue: armed between frames, applied at the next frame boundary by raising the
// guest's own scene request, which CoreLoop consumes through its scene loader.
class SceneWarp {
public:
  // The scene request word's "none pending" value.
  static constexpr std::uint32_t kNoRequest = 0xFFFFFFFFu;

  // The returned text answers the client. A scene whose table row is empty is refused.
  std::string arm(std::uint32_t scene, Core &core, const FrameProgram &program);
  // Re-enters the current scene at its spawn point `spawn`; refused while no level header is resident. The current
  // scene is the one loaded now, which the game may have changed itself (Crash 2's scene 0x23 runs into 0x3B).
  std::string armSpawn(std::uint32_t spawn, Core &core, const FrameProgram &program);
  // Raises the armed request when the guest has none pending; returns the scene applied.
  std::optional<std::uint32_t> apply(Core &core, const FrameProgram &program);
  // The scene table has a non-empty row for `scene`; an empty row hangs or faults the guest's load.
  static bool hasScene(Core &core, const SceneTable &table, std::uint32_t scene);

private:
  std::optional<std::uint32_t> armed_;
  std::optional<std::uint32_t> spawn_;
};

} // namespace crash
