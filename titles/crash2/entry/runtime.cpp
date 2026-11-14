#include "crash2/entry/runtime.h"

#include "crash2/entry/facts.h"
#include "crash2/entry/session.h"
#include "crash2/frame/camera_cuts.h"
#include "crash2/frame/pose_blend.h"
#include "crash2/render/morph_reader.h"
#include "crash2/render/world_renderer.h"
#include "game.h"

namespace crash2 {

Runtime::Runtime() : crash::Runtime(crash2::facts()) {}

void Runtime::registerOverrides(Game &game) {
  crash::Runtime::registerOverrides(game);
  DrawReplay::registerOverrides(game.core);
  camera_cuts::registerOverrides(game.core);
  pose_blend::registerOverride(game.core);
  MorphReader::registerOverrides(game.core);
  WorldRenderer::registerOverrides(game.core);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  return std::make_unique<Session>(facts());
}

psx::present::ReplayFrame *Runtime::replayFrame(Core &core) {
  return &session(core).drawReplay.frame();
}

} // namespace crash2
