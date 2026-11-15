#include "replay/draw_replay.h"

#include "core.h"
#include "native_dispatch.h"
#include "runtime/session.h"

namespace crash {

DrawReplay::DrawReplay(const ReplayFacts &facts)
    : facts_(facts), tracks_(facts.tracks), view_(facts), fade_(facts), walk_(facts), section_(facts),
      frame_(section_, tracks_.tracks(), sources_) {}

std::optional<psx::present::MotionRule::Samples> DrawReplay::pairedObject(std::uint32_t object) const {
  for (const psx::present::MotionTrack &track : tracks_.objectTracks()) {
    if (auto samples = frame_.motion().pairedRecord(track, object)) {
      return samples;
    }
  }
  return std::nullopt;
}

void DrawReplay::sampleOverride(Core *core) {
  session(*core).drawReplay().frame().beforeDraw(*core);
}

void DrawReplay::updateViewOverride(Core *core) {
  DrawReplay &replay = session(*core).drawReplay();
  if (!replay.frame().replaying()) {
    replay.view_.record(*core);
  }
  psx::cpu::callOriginalToReturn(
      *core, replay.facts_.updateView, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash view entry");
}

void DrawReplay::drawFadeOverride(Core *core) {
  DrawReplay &replay = session(*core).drawReplay();
  replay.fade_.draw(*core, replay.frame().replaying());
}

void DrawReplay::registerOverrides(Core &core, const ReplayFacts &facts) {
  psx::cpu::installNativeOverride(core, facts.sampleStub, "Crash draw replay sample", sampleOverride);
  psx::cpu::installNativeOverride(core, facts.updateView, "Crash view entry", updateViewOverride);
  psx::cpu::installNativeOverride(core, facts.drawFade, "Crash fade level", drawFadeOverride);
  ObjectWalk::registerOverrides(core, facts);
}

} // namespace crash
