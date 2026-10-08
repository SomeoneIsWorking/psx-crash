#pragma once

#include "native_frame_loop_contract.h"
#include "refusing_frame_driver.h"

namespace crash2 {

class Crash2FrameDriver final : public crash::RefusingFrameDriver<Crash2FrameDriver> {
public:
  static const crash::NativeFrameLoopContract &contract();
};

} // namespace crash2
