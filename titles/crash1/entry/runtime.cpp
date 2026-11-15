#include "crash1/entry/runtime.h"

#include "crash1/entry/facts.h"
#include "crash1/entry/session.h"
#include "crash1/frame/frame_driver.h"
#include "crash1/render/object_cull.h"
#include "game.h"

namespace crash1 {

Runtime::Runtime() : crash::Runtime(crash1::facts()) {}

void Runtime::registerOverrides(Game &game) {
  crash::Runtime::registerOverrides(game);
  BlockPool::registerOverride(game.core);
  ObjectCull::registerOverride(game.core);
  DrawReplay::registerOverrides(game.core);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  return std::make_unique<Session>(facts());
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(
      game, crash::session(game.core).frameCut, facts().frame, facts().vsync.begin, FrameDriver::executeHostTurn);
}

psx::present::ReplayFrame *Runtime::replayFrame(Core &core) {
  return &session(core).drawReplay.frame();
}

void Runtime::reportRun(Core &core) const {
  session(core).drawReplay.poses().report();
  session(core).drawReplay.screen().report();
}

} // namespace crash1
