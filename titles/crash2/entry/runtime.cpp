#include "crash2/entry/runtime.h"

#include "crash2/entry/facts.h"
#include "crash2/entry/session.h"
#include "game.h"

namespace crash2 {

Runtime::Runtime() : crash::Runtime(crash2::facts()) {}

void Runtime::registerOverrides(Game &game) {
  crash::Runtime::registerOverrides(game);
  DrawReplay::registerOverride(game.core);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  return std::make_unique<Session>(facts());
}

psx::present::ReplayFrame *Runtime::replayFrame(Core &core) {
  return &session(core).drawReplay.frame();
}

} // namespace crash2
