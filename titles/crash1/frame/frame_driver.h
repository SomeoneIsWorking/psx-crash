#pragma once

#include "frame/frame_driver.h"

namespace crash1 {

// The shared frame turn, with the host pad published through Crash 1's BIOS PadRead word.
class FrameDriver final : public crash::FrameDriver {
public:
  using crash::FrameDriver::FrameDriver;

protected:
  void publishInput(Core &core) override;
};

} // namespace crash1
