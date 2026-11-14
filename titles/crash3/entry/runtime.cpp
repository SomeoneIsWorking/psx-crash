#include "crash3/entry/runtime.h"

#include "crash3/entry/facts.h"
#include "crash3/render/world_renderer.h"
#include "game.h"

namespace crash3 {

Runtime::Runtime() : crash::Runtime(crash3::facts()) {}

void Runtime::registerOverrides(Game &game) {
  crash::Runtime::registerOverrides(game);
  WorldRenderer::registerOverrides(game.core);
}

} // namespace crash3
