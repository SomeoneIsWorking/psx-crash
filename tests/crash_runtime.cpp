#include "core.h"
#include "crash_titles.h"
#include "game.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "title_runtime_contract.h"

#include <cstdio>
#include <memory>

namespace {

int verifyInstalledOverrides(const char *name, GameRuntime &runtime, const crash::TitleFacts &facts) {
  auto game = std::make_unique<Game>();
  const auto identity = game->core.imageCatalog().activate(name, runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  const crash::FrameProgram &program = facts.frame;

  // The VSync calls are boundaries by return address; an override on the call site would hide them.
  if (game->core.nativeDispatcher().isInstalled({identity, program.firstVSync}) ||
      game->core.nativeDispatcher().isInstalled({identity, program.secondVSync})) {
    return crash_test::fail(name, "a function override sits on a VSync JAL call site");
  }
  for (const std::uint32_t owner : {facts.libcdInit.initialize.begin,
                                    facts.stockLibcd.control.begin,
                                    facts.stockLibcd.read.begin,
                                    facts.callbackBoot.initialize.begin,
                                    facts.gpuWatchdog.start.begin,
                                    facts.gpuWatchdog.check.begin,
                                    program.setRootCounter,
                                    program.startRootCounter,
                                    program.stopRootCounter}) {
    if (!game->core.nativeDispatcher().isInstalled({identity, owner})) {
      return crash_test::fail(name, "a shared native owner was not installed");
    }
  }

  crash::initializeNativeFrameLoopContract(*game);
  game->core.r[31] = program.afterFirstVSync;
  const auto vsync = psx::cpu::dispatchGuestHostService(game->core, facts.contract.guestVSync.begin);
  if (vsync.reason != psx::cpu::ExecutionExitReason::FrameBoundary || game->core.r[31] != program.afterFirstVSync) {
    return crash_test::fail(name, "the VSync host service did not preserve its typed continuation boundary");
  }

  game->core.r[31] = program.transitionReturn;
  const auto result = game->core.nativeDispatcher().invoke({identity, program.transition.begin});
  if (!result || result->reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    return crash_test::fail(name, "the transition override did not produce a typed frame boundary");
  }
  return 0;
}

} // namespace

int main() {
  const ExpectedRender record{RenderPath::Record, true};
  const auto state = crash::NativeFrameLoopState::FiniteBootSeamOnly;
  if (verifyTitleRuntimeContract<crash1::Crash1Runtime>("Crash1Runtime", true, state, record) != 0 ||
      verifyTitleRuntimeContract<crash2::Crash2Runtime>("Crash2Runtime", true, state, record) != 0) {
    return 1;
  }
  crash1::Crash1Runtime first;
  psxport_install_game(first);
  int failures = verifyInstalledOverrides("Crash1Runtime", first, first.facts());
  crash2::Crash2Runtime second;
  psxport_install_game(second);
  failures += verifyInstalledOverrides("Crash2Runtime", second, second.facts());
  std::printf("Crash runtimes: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
