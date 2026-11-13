#include "crash2/frame/camera_cuts.h"

#include "core.h"
#include "crash2/entry/session.h"
#include "native_dispatch.h"

#include <cstdint>
#include <lucent/log.h>
#include <string_view>

namespace crash2::camera_cuts {
namespace {

// Names from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/camera-cuts.md).
constexpr std::uint32_t kPreparedCut = 0x80022E74u;
constexpr std::uint32_t kZoneWalk = 0x80022F58u;
constexpr std::uint32_t kLoadCheckpoint = 0x8002F6C8u;
// The byte Cam_ProcessMessages sets for a prepared cut's go message; Cam_Update clears it each frame.
constexpr std::uint32_t kCutGo = 0x8005B99Bu;

void runOriginal(Core &core, std::uint32_t entry, std::string_view owner) {
  psx::cpu::callOriginalToReturn(core, entry, psx::cpu::ExecutionBudget::currentTurn(core), owner);
}

void preparedCut(Core *core) {
  // Without the go message the handler only follows the current path.
  if (core->mem_r8(kCutGo) != 0u) {
    lucent::debug("cut", "declared: prepared camera cut");
    session(*core).frameCut.declare();
  }
  runOriginal(*core, kPreparedCut, "Crash 2 prepared camera cut");
}

void zoneWalk(Core *core) {
  lucent::debug("cut", "declared: camera zone walk");
  session(*core).frameCut.declare();
  runOriginal(*core, kZoneWalk, "Crash 2 camera zone walk");
}

void loadCheckpoint(Core *core) {
  lucent::debug("cut", "declared: checkpoint camera");
  session(*core).frameCut.declare();
  runOriginal(*core, kLoadCheckpoint, "Crash 2 checkpoint camera");
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kPreparedCut, "Crash 2 prepared camera cut", preparedCut);
  psx::cpu::installNativeOverride(core, kZoneWalk, "Crash 2 camera zone walk", zoneWalk);
  psx::cpu::installNativeOverride(core, kLoadCheckpoint, "Crash 2 checkpoint camera", loadCheckpoint);
}

} // namespace crash2::camera_cuts
