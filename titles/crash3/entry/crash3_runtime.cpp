#include "crash3_runtime.h"

#include "crash3_frame_driver.h"
#include "platform_hle.h"

#include <memory>

namespace crash3 {

const GuestProgramImage Crash3Runtime::programImage_{
    .bss = {0x80060A38u, 0x8006EA78u},
    .stackTopWordAddress = 0x80060884u,
    .stackReserveWordAddress = 0x80060880u,
    .heapBase = 0x8006EA78u,
    .heapSizeStoreAddress = 0x8005E1ACu,
    .heapBaseStoreAddress = 0x8005E1A8u,
    .globalPointer = 0x80060878u,
    .libcInitEntry = 0x800112B8u,
    .gameMainEntry = 0x80048AA0u,
    .crt0Entry = 0x800489F8u,
    .residentText = {0x00010000u, 0x00061000u},
    .backtraceText = {},
    .stackBias = {true, -8},
};

const PlatformHlePlan Crash3Runtime::platformPlan_ = crash::makeNativeFramePlatformPlan(Crash3FrameDriver::contract());

Crash3Runtime::Crash3Runtime()
    : BoundaryRuntime("crash3-runtime",
                      "native boot is unavailable: Crash 3 has no title-owned Lightrec boot services "
                      "or measured advancing frame driver; Crash 1 remains the active title") {}

const GuestProgramImage *Crash3Runtime::guestProgramImage() const {
  return &programImage_;
}

const PlatformHlePlan *Crash3Runtime::platformHlePlan() const {
  return &platformPlan_;
}

const GuestWidescreenProjection *Crash3Runtime::guestWidescreenProjection() const {
  // `BoundaryRuntime::registerOverrides` is final, so no projection override can be installed.
  return &widescreen_;
}

bool Crash3Runtime::guestVramIsPicture(const Game &) const {
  return false;
}

std::unique_ptr<FrameDriver> Crash3Runtime::createFrameDriver(Game &) {
  return std::make_unique<Crash3FrameDriver>();
}

const crash::NativeFrameLoopContract &Crash3Runtime::nativeFrameLoopContract() const {
  return Crash3FrameDriver::contract();
}

} // namespace crash3
