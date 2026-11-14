#include "frame/scene_warp.h"

#include "core.h"

#include <lucent/log.h>

namespace crash {

bool SceneWarp::hasScene(Core &core, const SceneTable &table, std::uint32_t scene) {
  if (scene >= table.rows) {
    return false;
  }
  return (core.mem_r32(table.address + scene * table.rowBytes + 4u) & table.sizeMask) != 0u;
}

std::string SceneWarp::arm(std::uint32_t scene, Core &core, const FrameProgram &program) {
  if (!hasScene(core, program.sceneTable, scene)) {
    return lucent::format("refused: scene 0x{:X} has no row in the scene table", scene);
  }
  armed_ = scene;
  spawn_.reset();
  return lucent::format("ok: warp armed for scene 0x{:X}", scene);
}

std::string SceneWarp::armSpawn(std::uint32_t spawn, Core &core, const FrameProgram &program) {
  if (!program.spawns) {
    return "refused: this title's spawn points are not measured";
  }
  const std::uint32_t header = core.mem_r32(program.spawns->levelHeader);
  if (header == 0u) {
    return "refused: no level is loaded";
  }
  const std::uint32_t count = core.mem_r32(header);
  if (spawn >= count) {
    return lucent::format("refused: the level has spawn points 0-{}", count - 1u);
  }
  const std::uint32_t level = core.mem_r32(header + program.spawns->levelIdOffset);
  if (level == program.spawns->warpRoomLevel) {
    return "refused: the warp room picks its spawn point from the level left";
  }
  if (spawn >= program.spawns->carriedSpawns) {
    return lucent::format(
        "refused: the game starts level 0x{:X} only at spawn points 0-{}", level, program.spawns->carriedSpawns - 1u);
  }
  const std::uint32_t scene = core.mem_r32(program.sceneIdAddress);
  armed_ = scene;
  spawn_ = spawn;
  return lucent::format("ok: warp armed for scene 0x{:X} spawn {}", scene, spawn);
}

std::optional<std::uint32_t> SceneWarp::apply(Core &core, const FrameProgram &program) {
  if (!armed_ || core.mem_r32(program.sceneRequestAddress) != kNoRequest) {
    return std::nullopt;
  }
  const std::uint32_t scene = *armed_;
  armed_.reset();
  if (spawn_) {
    core.mem_w32(program.spawns->spawnIndex, *spawn_);
  }
  core.mem_w32(program.sceneRequestAddress, scene);
  lucent::info("crash-warp", "scene 0x{:X} spawn {} requested", scene, spawn_.value_or(0u));
  spawn_.reset();
  return scene;
}

} // namespace crash
