#pragma once

#include "crash1_block_pool.h"
#include "crash1_frame_cut.h"
#include "crash1_horizontal_bound.h"
#include "crash1_widescreen.h"
#include "game_runtime.h"
#include "gpu_vk.h"
#include "native_frame_loop_contract.h"

#include <memory>

namespace crash1 {

// Process-lifetime owner of Crash 1's boot prefix and host frame loop.
class Crash1Runtime final : public GameRuntime {
public:
  Crash1Runtime();

  RenderCapabilities renderCapabilities() const override;
  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  bool guestVramIsPicture(const Game &game) const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  bool sealedFrameIsCut(Core &core) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  const char *discEnvVar() const override;
  void reportRun(Core &core) const override;

  const crash::NativeFrameLoopContract &nativeFrameLoopContract() const;

  // Projection owner handed to the framework's latch.
  Crash1Widescreen &widescreen() {
    return widescreen_;
  }
  const Crash1Widescreen &widescreen() const {
    return widescreen_;
  }

  // Horizontal-bound owner; checks H stayed fixed at the guest's consumer.
  Crash1HorizontalBound &horizontalBound() {
    return horizontalBound_;
  }
  const Crash1HorizontalBound &horizontalBound() const {
    return horizontalBound_;
  }

  // Size-class block-pool owner (holds the product's stop address).
  Crash1BlockPool &blockPool() {
    return blockPool_;
  }
  const Crash1BlockPool &blockPool() const {
    return blockPool_;
  }

private:
  static const GuestProgramImage programImage_;
  static const PlatformHlePlan platformPlan_;
  Crash1Widescreen widescreen_{Crash1Widescreen::facts(), &gpu_vk_latch_guest_projection};
  Crash1HorizontalBound horizontalBound_{};
  Crash1BlockPool blockPool_{};
  Crash1FrameCut frameCut_{};
};

} // namespace crash1
