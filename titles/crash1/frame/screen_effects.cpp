#include "crash1/frame/screen_effects.h"

#include "blend.h"
#include "core.h"
#include "crash1/entry/session.h"
#include "crash1/frame/frame_facts.h"
#include "native_dispatch.h"

#include <algorithm>
#include <lucent/log.h>

namespace crash1 {

void ScreenEffects::openPass() {
  drawnFade_ = drawingFade_;
  drawingFade_.reset();
}

void ScreenEffects::report() const {
  lucent::info("crash1-replay", "fades: {} drawn as the newest tick (none the tick before)", unpairedFades_);
}

void ScreenEffects::matrices(Core &core, bool replaying) {
  if (replaying) {
    core.mem_w32(frame_facts::kScreenShake, shake_);
  } else {
    shake_ = core.mem_r32(frame_facts::kScreenShake);
  }
  psx::cpu::callOriginalToReturn(
      core, frame_facts::kUpdateMatrices, psx::cpu::ExecutionBudget::currentTurn(core), "Crash 1 view matrices");
}

void ScreenEffects::overlay(Core &core, std::optional<float> t) {
  const auto original = [&core] {
    psx::cpu::callOriginalToReturn(
        core, frame_facts::kDrawOverlay, psx::cpu::ExecutionBudget::currentTurn(core), "Crash 1 fade overlay");
  };
  if (std::ranges::find(frame_facts::kFadeReturns, core.r[31]) == frame_facts::kFadeReturns.end()) {
    original();
    return;
  }
  if (!t) {
    drawingFade_ = static_cast<std::int32_t>(core.r[4]);
    original();
    return;
  }
  // The replay runs before the newest tick's Gpu_Update and steps the fade to that tick's value itself.
  if (drawnFade_) {
    core.r[4] =
        static_cast<std::uint32_t>(psx::present::lerpInt(*drawnFade_, static_cast<std::int32_t>(core.r[4]), *t));
  } else {
    ++unpairedFades_;
  }
  original();
}

void ScreenEffects::matricesOverride(Core *core) {
  DrawReplay &replay = session(*core).drawReplay;
  replay.screen().matrices(*core, replay.frame().replaying().has_value());
}

void ScreenEffects::overlayOverride(Core *core) {
  DrawReplay &replay = session(*core).drawReplay;
  replay.screen().overlay(*core, replay.frame().replaying());
}

void ScreenEffects::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, frame_facts::kUpdateMatrices, "Crash 1 screen shake hold", matricesOverride);
  psx::cpu::installNativeOverride(core, frame_facts::kDrawOverlay, "Crash 1 fade blend", overlayOverride);
}

} // namespace crash1
