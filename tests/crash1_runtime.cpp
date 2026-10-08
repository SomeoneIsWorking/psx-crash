#include "crash1_runtime.h"
#include "core.h"
#include "crash1_frame_driver.h"
#include "game.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "title_runtime_contract.h"

#include <cstdio>
#include <memory>

int main() {
  if (verifyTitleRuntimeContract<crash1::Crash1Runtime>("Crash1Runtime",
                                                        true,
                                                        crash::NativeFrameLoopState::FiniteBootSeamOnly,
                                                        ExpectedRender{RenderPath::Record, true}) != 0) {
    return 1;
  }

  crash1::Crash1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  const auto identity =
      game->core.imageCatalog().activate("Crash1Runtime", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);

  const crash1::Crash1FrameProgram &program = crash1::Crash1FrameDriver::program();
  if (game->core.nativeDispatcher().isInstalled({identity, program.firstVSync}) ||
      game->core.nativeDispatcher().isInstalled({identity, program.secondVSync})) {
    std::fprintf(stderr, "Crash1Runtime incorrectly installed a function override on a VSync JAL call site\n");
    return 1;
  }

  crash::initializeNativeFrameLoopContract(*game);
  game->core.r[31] = program.afterFirstVSync;
  const auto vsync = psx::cpu::dispatchGuestHostService(game->core, runtime.nativeFrameLoopContract().guestVSync.begin);
  if (vsync.reason != psx::cpu::ExecutionExitReason::FrameBoundary || game->core.r[31] != program.afterFirstVSync) {
    std::fprintf(stderr, "Crash1Runtime VSync host service did not preserve its typed continuation boundary\n");
    return 1;
  }

  const std::uint32_t transition = program.transition.begin;
  game->core.r[31] = transition;
  const auto result = game->core.nativeDispatcher().invoke({identity, transition});
  if (!result || result->reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    std::fprintf(stderr, "Crash1Runtime transition override did not produce a typed frame boundary\n");
    return 1;
  }
  std::printf("Crash1Runtime: transition override produced a typed frame boundary\n");
  return 0;
}
