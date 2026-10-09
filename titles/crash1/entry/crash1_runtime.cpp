#include "crash1_runtime.h"

#include "crash1_facts.h"
#include "crash1_frame_driver.h"
#include "game.h"

namespace crash1 {

Crash1Runtime::Crash1Runtime() : CrashRuntime(crash1::facts(), widescreen_, "crash1") {}

void Crash1Runtime::registerTitleOverrides(Game &game) {
  installCrash1HorizontalBound(game.core);
  installCrash1BlockPool(game.core);
}

std::unique_ptr<FrameDriver> Crash1Runtime::createFrameDriver(Game &game) {
  frameCut().reset();
  return std::make_unique<Crash1FrameDriver>(game, frameCut(), facts().frame, facts().contract);
}

} // namespace crash1
