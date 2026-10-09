#pragma once

#include "crash2_widescreen.h"
#include "crash_runtime.h"
#include "gpu_vk.h"

namespace crash2 {

// Crash 2: the shared Crash runtime over its facts.
class Crash2Runtime final : public crash::CrashRuntime {
public:
  Crash2Runtime();

  Crash2Widescreen &widescreen() {
    return widescreen_;
  }
  const Crash2Widescreen &widescreen() const {
    return widescreen_;
  }

protected:
  void registerTitleOverrides(Game &game) override;

private:
  Crash2Widescreen widescreen_{Crash2Widescreen::facts(), &gpu_vk_latch_guest_projection};
};

} // namespace crash2
