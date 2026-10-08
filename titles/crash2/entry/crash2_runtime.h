#pragma once

#include "boundary_runtime.h"
#include "crash2_widescreen.h"
#include "gpu_vk.h"
#include "native_frame_loop_contract.h"

#include <memory>

namespace crash2 {

// Owner of Crash 2's measured executable facts.
class Crash2Runtime final : public crash::BoundaryRuntime {
public:
  Crash2Runtime();

  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  bool guestVramIsPicture(const Game &game) const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;

  const crash::NativeFrameLoopContract &nativeFrameLoopContract() const;

  // Projection owner handed to the framework's latch.
  Crash2Widescreen &widescreen() {
    return widescreen_;
  }
  const Crash2Widescreen &widescreen() const {
    return widescreen_;
  }

private:
  static const GuestProgramImage programImage_;
  static const PlatformHlePlan platformPlan_;
  Crash2Widescreen widescreen_{Crash2Widescreen::facts(), &gpu_vk_latch_guest_projection};
};

} // namespace crash2
