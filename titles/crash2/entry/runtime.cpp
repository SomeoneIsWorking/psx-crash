#include "crash2/entry/runtime.h"

#include "crash2/entry/facts.h"
#include "crash2/frame/camera_cuts.h"
#include "crash2/render/world_renderer.h"
#include "game.h"

namespace crash2 {

Runtime::Runtime() : crash::Runtime(crash2::facts()) {}

void Runtime::registerOverrides(Game &game) {
  crash::Runtime::registerOverrides(game);
  camera_cuts::registerOverrides(game.core);
  WorldRenderer::registerOverrides(game.core);
}

} // namespace crash2
