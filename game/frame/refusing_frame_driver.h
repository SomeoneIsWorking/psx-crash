#pragma once

#include "game_runtime.h"
#include "native_frame_loop_contract.h"

namespace crash {

// Frame driver for titles whose frame frontier is not ready: a host frame request is fatal.
template <typename TitleDriver> class RefusingFrameDriver : public FrameDriver {
public:
  void stepFrame(Core &, std::uint32_t frame) final {
    abortUnprovenFrameStep(TitleDriver::contract(), frame);
  }
};

} // namespace crash
