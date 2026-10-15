#include "frame/scene_warp.h"

#include "core.h"

#include <charconv>
#include <lucent/log.h>
#include <string_view>

namespace crash {
namespace {

// The hex scene id after the command word; an optional 0x prefix is accepted.
std::optional<std::uint32_t> parseScene(std::string_view line) {
  const std::size_t start = line.find_first_not_of(' ', line.find(' '));
  if (start == std::string_view::npos) {
    return std::nullopt;
  }
  std::string_view digits = line.substr(start);
  if (digits.starts_with("0x") || digits.starts_with("0X")) {
    digits.remove_prefix(2);
  }
  std::uint32_t scene = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), scene, 16);
  if (error != std::errc{} || end == digits.data()) {
    return std::nullopt;
  }
  return scene;
}

} // namespace

bool SceneWarp::hasScene(Core &core, const SceneTable &table, std::uint32_t scene) {
  if (table.address == 0u) {
    return true;
  }
  if (scene >= table.rows) {
    return false;
  }
  return (core.mem_r32(table.address + scene * table.rowBytes + 4u) & table.sizeMask) != 0u;
}

std::string SceneWarp::arm(const char *line, bool loopRunning, Core &core, const FrameProgram &program) {
  const std::optional<std::uint32_t> parsed = parseScene(line);
  if (!parsed) {
    return "usage: warp <scene-hex>";
  }
  const std::uint32_t scene = *parsed;
  if (scene > kLastScene) {
    return lucent::format("refused: scene 0x{:X} is past the last scene 0x{:X}", scene, kLastScene);
  }
  if (!loopRunning) {
    return "refused: a warp is legal only once the game loop is running";
  }
  if (!hasScene(core, program.sceneTable, scene)) {
    return lucent::format("refused: scene 0x{:X} has no row in the scene table", scene);
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
