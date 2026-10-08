#include "native_frame_loop_contract.h"

#include "game.h"
#include "platform_hle.h"

#include <lucent/log.h>

#include <cstdlib>

namespace crash {

[[noreturn]] void abortUnprovenFrameStep(const NativeFrameLoopContract &contract, std::uint32_t frame) {
  lucent::error("crash-frame",
                "{} native frame {} refused: {} Guest VSync [0x{:08X},0x{:08X}) must never execute; "
                "the host driver requires a measured frame boundary first.",
                contract.codeword,
                frame,
                contract.refusal,
                contract.guestVSync.begin,
                contract.guestVSync.end);
  std::abort();
}

PlatformHlePlan makeNativeFramePlatformPlan(const NativeFrameLoopContract &contract) {
  PlatformHlePlan plan{};
  plan.vsyncAddress = contract.guestVSync.begin;
  plan.vsyncQueryCounterAddress = contract.vsyncQueryCounter;
  plan.windowLo[0] = contract.guestVSync.begin;
  plan.windowHi[0] = contract.guestVSync.end;
  return plan;
}

void initializeNativeFrameLoopContract(Game &game) {
  game.platform_hle.initBuiltins();
  game.platform_hle.requireNativeFrameLoopContract();
}

} // namespace crash
