#pragma once

#include "crash_frame_driver.h"

namespace crash1 {

// The shared frame turn, with the host pad published through Crash 1's BIOS PadRead word.
class Crash1FrameDriver final : public crash::CrashFrameDriver {
public:
  using CrashFrameDriver::CrashFrameDriver;

protected:
  void publishInput(Core &core) override;
};

} // namespace crash1
