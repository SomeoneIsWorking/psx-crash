#include "scene_warp.h"

#include "core.h"

#include <cstdio>
#include <lucent/log.h>

namespace crash {

std::string SceneWarp::arm(const char *line, bool loopRunning) {
  unsigned scene = 0;
  if (std::sscanf(line, "%*s %x", &scene) != 1) {
    return "usage: warp <scene-hex>";
  }
  if (scene > kLastScene) {
    return lucent::format("refused: scene 0x{:X} is past the last scene 0x{:X}", scene, kLastScene);
  }
  if (!loopRunning) {
    return "refused: a warp is legal only once the game loop is running";
  }
  armed_ = scene;
  return lucent::format("ok: warp armed for scene 0x{:X}", scene);
}

std::optional<std::uint32_t> SceneWarp::apply(Core &core, const FrameProgram &program) {
  if (!armed_ || core.mem_r32(program.sceneRequestAddress) != kNoRequest) {
    return std::nullopt;
  }
  const std::uint32_t scene = *armed_;
  armed_.reset();
  core.mem_w32(program.sceneRequestAddress, scene);
  lucent::info("crash-warp", "scene 0x{:X} requested", scene);
  return scene;
}

} // namespace crash
