#pragma once

#include "crash1_block_pool.h"
#include "crash1_horizontal_bound.h"
#include "crash1_widescreen.h"
#include "crash_runtime.h"
#include "gpu_vk.h"

#include <memory>

namespace crash1 {

// Crash 1: the shared Crash runtime over its facts, plus its horizontal bound, block pool and BIOS pad word.
class Crash1Runtime final : public crash::CrashRuntime {
public:
  Crash1Runtime();

  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;

  Crash1Widescreen &widescreen() {
    return widescreen_;
  }
  const Crash1Widescreen &widescreen() const {
    return widescreen_;
  }
  Crash1HorizontalBound &horizontalBound() {
    return horizontalBound_;
  }
  const Crash1HorizontalBound &horizontalBound() const {
    return horizontalBound_;
  }
  Crash1BlockPool &blockPool() {
    return blockPool_;
  }
  const Crash1BlockPool &blockPool() const {
    return blockPool_;
  }

protected:
  void registerTitleOverrides(Game &game) override;

private:
  Crash1Widescreen widescreen_{Crash1Widescreen::facts(), &gpu_vk_latch_guest_projection};
  Crash1HorizontalBound horizontalBound_{};
  Crash1BlockPool blockPool_{};
};

} // namespace crash1
