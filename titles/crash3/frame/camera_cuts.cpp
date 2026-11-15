#include "crash3/frame/camera_cuts.h"

#include "core.h"
#include "native_dispatch.h"
#include "runtime/session.h"

#include <cstdint>
#include <lucent/log.h>

namespace crash3::camera_cuts {
namespace {

// Sys_LoadCheckpoint (crash3-decomp docs/notes/frame-loop.md): the zone, camera path and progress from a continue
// block. Sys_MainLoop calls it for a scene request of -2; 0x800307F4 and 0x80035D08 call it without one.
constexpr std::uint32_t kLoadCheckpoint = 0x80030388u;

void loadCheckpoint(Core *core) {
  lucent::debug("cut", "declared: checkpoint camera");
  crash::session(*core).frameCut.declare();
  // A tail call: a budget exit (a checkpoint's entry loads can outrun the turn) is the caller's, resumed next turn.
  (void)psx::cpu::callOriginalOrPropagate(*core, kLoadCheckpoint);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kLoadCheckpoint, "Crash 3 checkpoint camera", loadCheckpoint);
}

} // namespace crash3::camera_cuts
