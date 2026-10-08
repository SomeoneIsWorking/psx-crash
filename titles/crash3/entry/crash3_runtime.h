#pragma once

#include "boundary_runtime.h"
#include "crash3_widescreen.h"
#include "gpu_vk.h"
#include "native_frame_loop_contract.h"

#include <memory>

namespace crash3 {

// Owner of SCUS-94244's measured executable facts.
class Crash3Runtime final : public crash::BoundaryRuntime {
public:
  Crash3Runtime();

  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  bool guestVramIsPicture(const Game &game) const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;

  const crash::NativeFrameLoopContract &nativeFrameLoopContract() const;

  // Projection owner handed to the framework's latch.
  Crash3Widescreen &widescreen() {
    return widescreen_;
  }
  const Crash3Widescreen &widescreen() const {
    return widescreen_;
  }

private:
  static const GuestProgramImage programImage_;
  static const PlatformHlePlan platformPlan_;
  Crash3Widescreen widescreen_{Crash3Widescreen::facts(), &gpu_vk_latch_guest_projection};
};

} // namespace crash3
