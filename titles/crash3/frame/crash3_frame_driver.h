#pragma once

#include "native_frame_loop_contract.h"
#include "refusing_frame_driver.h"

namespace crash3 {

class Crash3FrameDriver final : public crash::RefusingFrameDriver<Crash3FrameDriver> {
public:
  static const crash::NativeFrameLoopContract &contract();
};

} // namespace crash3
