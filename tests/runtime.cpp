#include "core.h"
#include "game.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "titles.h"

#include <cstdio>
#include <vector>

namespace {

// The shared runtime binds every native owner at the title's own addresses and frames on its VSync.
int verifyRegisteredOverrides(const char *title, Game &game, const crash::TitleFacts &facts) {
  Core &core = game.core;
  const GuestAddressRange resident{0x00010000u, facts.crt0.bss.begin & 0x1FFFFFFFu};
  const auto identity = core.imageCatalog().activate(title, resident, 1u);
  game.runtime.registerOverrides(game);
  const crash::FrameProgram &program = facts.frame;
  int bad = 0;

  // The VSync calls are boundaries by return address; an override on the call site would hide them.
  if (core.nativeDispatcher().isInstalled({identity, program.firstVSync}) ||
      core.nativeDispatcher().isInstalled({identity, program.secondVSync})) {
    bad += crash_test::fail(title, "a function override sits on a VSync JAL call site");
  }
  // Stock libcd binds into the platform HLE table at the title's entries.
  for (const std::uint32_t entry : {facts.stockLibcd.control,
                                    facts.stockLibcd.controlFire,
                                    facts.stockLibcd.sync,
                                    facts.stockLibcd.read,
                                    facts.stockLibcd.readSync}) {
    if (game.platform_hle.lookup(entry) == nullptr) {
      std::fprintf(stderr, "FAIL %s: no stock libcd entry at 0x%08X\n", title, entry);
      ++bad;
    }
  }
  if (game.cd.stockLibrary() == nullptr) {
    bad += crash_test::fail(title, "the runtime's libcd is not the Game's stock library");
  }
  std::vector<std::uint32_t> owners{facts.libcdInit.initialize.begin,
                                    facts.callbackBoot.initialize.begin,
                                    program.setRootCounter,
                                    program.startRootCounter,
                                    program.stopRootCounter,
                                    program.transition.begin};
  if (facts.gpuWatchdog) {
    owners.push_back(facts.gpuWatchdog->start.begin);
    owners.push_back(facts.gpuWatchdog->check.begin);
  }
  for (const std::uint32_t owner : owners) {
    if (!core.nativeDispatcher().isInstalled({identity, owner})) {
      std::fprintf(stderr, "FAIL %s: no native owner at 0x%08X\n", title, owner);
      ++bad;
    }
  }

  core.r[31] = program.afterFirstVSync;
  const auto vsync = psx::cpu::dispatchGuestHostService(core, facts.vsync.begin);
  if (vsync.reason != psx::cpu::ExecutionExitReason::FrameBoundary || core.r[31] != program.afterFirstVSync) {
    bad += crash_test::fail(title, "the VSync host service did not preserve its typed continuation boundary");
  }

  core.r[31] = program.transitionReturn;
  const auto result = core.nativeDispatcher().invoke({identity, program.transition.begin});
  if (!result || result->reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    bad += crash_test::fail(title, "the transition override did not produce a typed frame boundary");
  }

  const RenderCapabilities render = game.runtime.renderCapabilities();
  if (render.defaultPath != RenderPath::Record || render.nativeRenderPath || game.runtime.guestVramIsPicture(game)) {
    bad += crash_test::fail(title, "a Crash title presents its recorded GP0 stream");
  }
  return bad;
}

} // namespace

int main() {
  const int failures = crash_test::forEachTitle(verifyRegisteredOverrides);
  std::printf("Crash runtimes: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
