#include "frame/camera_cuts.h"

#include "core.h"
#include "native_dispatch.h"
#include "runtime/session.h"

#include <lucent/log.h>

namespace crash {
namespace {

// A tail call: a budget exit (a checkpoint's entry loads can outrun the turn) is the caller's, resumed next turn.
void declareAndRun(Core &core, std::uint32_t entry, const char *what) {
  lucent::debug("cut", "declared: {}", what);
  session(core).frameCut.declare();
  (void)psx::cpu::callOriginalOrPropagate(core, entry);
}

} // namespace

void CameraCuts::preparedCutOverride(Core *core) {
  const CameraCutFacts &facts = session(*core).cameraCuts().facts_;
  // Without the go byte the handler only follows the current path.
  if (core->mem_r8(facts.cutGo) == 0u) {
    (void)psx::cpu::callOriginalOrPropagate(*core, facts.preparedCut);
    return;
  }
  declareAndRun(*core, facts.preparedCut, "prepared camera cut");
}

void CameraCuts::zoneWalkOverride(Core *core) {
  declareAndRun(*core, session(*core).cameraCuts().facts_.zoneWalk, "camera zone walk");
}

void CameraCuts::loadCheckpointOverride(Core *core) {
  declareAndRun(*core, session(*core).cameraCuts().facts_.loadCheckpoint, "checkpoint camera");
}

void CameraCuts::registerOverrides(Core &core, const CameraCutFacts &facts) {
  psx::cpu::installNativeOverride(core, facts.preparedCut, "Crash prepared camera cut", preparedCutOverride);
  psx::cpu::installNativeOverride(core, facts.zoneWalk, "Crash camera zone walk", zoneWalkOverride);
  psx::cpu::installNativeOverride(core, facts.loadCheckpoint, "Crash checkpoint camera", loadCheckpointOverride);
}

} // namespace crash
