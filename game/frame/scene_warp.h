#pragma once

#include "frame_program.h"

#include <cstdint>
#include <optional>
#include <string>

class Core;

namespace crash {

// The `warp <scene>` control request: armed between frames, applied at the next frame boundary by raising the
// guest's own scene request, which CoreLoop consumes through its scene loader.
class SceneWarp {
public:
  // Highest scene id the loader's scene tables cover; ids the disc has no scene for fault in the guest.
  static constexpr std::uint32_t kLastScene = 0x3Cu;
  // The scene request word's "none pending" value.
  static constexpr std::uint32_t kNoRequest = 0xFFFFFFFFu;

  // Parses one command line; the returned text answers the client.
  std::string arm(const char *line, bool loopRunning);
  // Raises the armed request when the guest has none pending; returns the scene applied.
  std::optional<std::uint32_t> apply(Core &core, const FrameProgram &program);

private:
  std::optional<std::uint32_t> armed_;
};

} // namespace crash
