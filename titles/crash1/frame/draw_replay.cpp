#include "crash1/frame/draw_replay.h"

#include "core.h"
#include "crash1/entry/session.h"
#include "crash1/frame/frame_facts.h"
#include "native_dispatch.h"

namespace crash1 {
namespace {

namespace facts = frame_facts;
namespace object = frame_facts::object;

constexpr crash::TrackFacts kTracks{.camera = facts::kCamera,
                                    .objectPool = facts::kObjectPool,
                                    .objectStride = facts::kObjectStride,
                                    .objectCount = facts::kObjectCount,
                                    .player = facts::kPlayer,
                                    .object = {.exec = object::kExec,
                                               .trans = object::kTrans,
                                               .rot = object::kRot,
                                               .scale = object::kScale,
                                               .anim = object::kAnimSeq,
                                               .animFrame = object::kAnimFrame}};

void updateObjectsOverride(Core *core) {
  const std::uint32_t returnAddress = core->r[31];
  DrawReplay &replay = session(*core).drawReplay;
  const bool real = returnAddress == facts::kObjectsReturn && !replay.frame().replaying();
  if (real) {
    replay.poses().openPass();
    replay.screen().openPass();
  }
  psx::cpu::callOriginalToReturn(
      *core, facts::kGoolUpdateObjects, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 1 object update");
  if (real) {
    replay.poses().closePass();
    replay.frame().beforeDraw(*core);
  }
}

// The frame's input was read by the real walk; a replay's walk reads none.
void padUpdateOverride(Core *core) {
  if (session(*core).drawReplay.frame().replaying()) {
    return;
  }
  psx::cpu::callOriginalToReturn(
      *core, facts::kPadUpdate, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 1 pad update");
}

} // namespace

DrawReplay::DrawReplay() : tracks_(kTracks), frame_(section_, tracks_.tracks()) {}

void DrawReplay::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, facts::kGoolUpdateObjects, "Crash 1 draw replay sample", updateObjectsOverride);
  psx::cpu::installNativeOverride(core, facts::kPadUpdate, "Crash 1 pad update", padUpdateOverride);
  PoseBlend::registerOverrides(core);
  ScreenEffects::registerOverrides(core);
}

} // namespace crash1
