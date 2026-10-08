#pragma once

#include "core.h"
#include "game.h"
#include "game_iface.h"
#include "image_identity.h"
#include "native_frame_loop_contract.h"
#include "platform_hle.h"

#include <cstdio>
#include <memory>
#include <type_traits>

// The render shape a title ships: the guest's geometry on the GTE path unless it moved to Record.
struct ExpectedRender {
  RenderPath defaultPath = RenderPath::Gte;
  bool temporalInterpolation = false;
};

template <typename Runtime>
int verifyTitleRuntimeContract(const char *name,
                               bool expectProgramImage,
                               crash::NativeFrameLoopState expectedFrameState,
                               ExpectedRender expectedRender = {}) {
  static_assert(std::is_base_of_v<GameRuntime, Runtime>);
  static_assert(!std::is_base_of_v<LegacyGameRuntimeAdapter, Runtime>);

  Runtime runtime;
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  if (psxport_game_runtime() != &runtime || game->runtime != &runtime || game->core.runtime != &runtime) {
    std::fprintf(stderr, "%s did not own the installed framework seam\n", name);
    return 1;
  }
  if (game->core.cfg != nullptr || game->core.hooks != nullptr || runtime.legacyConfigForMigration() != nullptr ||
      runtime.legacyHooksForMigration() != nullptr) {
    std::fprintf(stderr, "%s exposed a legacy GameConfig/GameHooks view\n", name);
    return 1;
  }
  if (game->core.gameCtx != nullptr || game->taskScheduler != nullptr) {
    std::fprintf(stderr, "%s invented an unmeasured context or scheduler\n", name);
    return 1;
  }
  if (game->frameDriver == nullptr) {
    std::fprintf(stderr, "%s did not create its mandatory host-owned frame driver\n", name);
    return 1;
  }
  if ((game->core.guestProgramImage != nullptr) != expectProgramImage) {
    std::fprintf(stderr, "%s installed the wrong GuestProgramImage ownership state\n", name);
    return 1;
  }
  if (runtime.guestVramIsPicture(*game)) {
    std::fprintf(stderr, "%s claimed guest VRAM was a picture without a measured frame producer\n", name);
    return 1;
  }
  const crash::NativeFrameLoopContract &frame = runtime.nativeFrameLoopContract();
  const PlatformHlePlan *platform = runtime.platformHlePlan();
  const bool expectRunnableFrame = expectedFrameState == crash::NativeFrameLoopState::Ready;
  if (!frame.guestVSync.valid() || frame.state != expectedFrameState || frame.canStepFrame() != expectRunnableFrame) {
    std::fprintf(stderr, "%s overstated or omitted its current native-frame frontier\n", name);
    return 1;
  }
  if (platform == nullptr || platform->vsyncAddress != frame.guestVSync.begin ||
      platform->windowLo[0] != frame.guestVSync.begin || platform->windowHi[0] != frame.guestVSync.end ||
      platform->bindingCount != 0) {
    std::fprintf(stderr, "%s did not bind its measured VSync leaf to the framework frame boundary\n", name);
    return 1;
  }
  const RenderCapabilities capabilities = runtime.renderCapabilities();
  if (capabilities.defaultPath != expectedRender.defaultPath || capabilities.nativeRenderPath ||
      capabilities.temporalInterpolation != expectedRender.temporalInterpolation ||
      capabilities.playerPathCount() != 1 ||
      render_path_resolve(RenderPath::Native, capabilities) != expectedRender.defaultPath ||
      render_path_resolve(RenderPath::Gte, capabilities) != RenderPath::Gte ||
      game->mods.temporalInterpolationSupported() != expectedRender.temporalInterpolation) {
    std::fprintf(stderr, "%s declared a render shape other than the one this title ships\n", name);
    return 1;
  }
  if (render_path_apply(*game, RenderPath::Native, RenderPathAudience::Player) !=
          RenderPathSelectionResult::Unsupported ||
      render_path_apply(*game, RenderPath::Gte, RenderPathAudience::Player) != RenderPathSelectionResult::Applied ||
      game->core.rsub.mode.path() != RenderPath::Gte) {
    std::fprintf(stderr, "%s did not enforce its guest-geometry player path at the live selector\n", name);
    return 1;
  }

  if (expectProgramImage) {
    game->core.imageCatalog().activate(name, runtime.guestProgramImage()->residentText, 1u);
    runtime.registerOverrides(*game);
  }
  std::printf("%s: direct derived install; host FrameDriver + typed VSync contract; guest-geometry path; "
              "%s\n",
              name,
              expectRunnableFrame ? "measured runnable frame seam" : "explicit non-runnable frontier");
  return 0;
}
