#pragma once

#include "boot/callback_boot.h"
#include "boot/gpu_watchdog.h"
#include "boot/libcd_init.h"
#include "crt0_boot.h"
#include "frame/frame_program.h"
#include "guest_address_range.h"
#include "runtime/guest_function_range.h"
#include "stock_libcd.h"
#include "stock_pad_buffers.h"

#include <cstdint>
#include <optional>

namespace crash {

// Everything the shared boot and frame owners need from one title's executable, measured in
// `titles/<title>/executable.json` and the decompilation each title's facts file cites.
struct TitleFacts {
  psx::game::Crt0Image crt0;
  // The retail C main's three steps before CoreLoop: static constructors, the CD flag, Init.
  std::uint32_t staticConstructors;
  std::uint32_t useCdAddress;
  std::uint32_t init;
  libcd_init::Program libcdInit;
  psx::cd::StockLibcd::Entries stockLibcd;
  callback_boot::Program callbackBoot;
  // Empty: the title's own watchdog body runs as guest code (Crash 3's differs from Crash 1 and 2).
  std::optional<gpu_watchdog::Program> gpuWatchdog;
  FrameProgram frame;
  // libetc VSync and the field counter it returns for a negative argument.
  GuestFunctionRange vsync;
  std::uint32_t vsyncQueryCounter;
  // Physical range the title streams code modules into through stock CdRead; empty when it loads none.
  GuestAddressRange codeModuleArena;
  // libpad InitPAD receive buffers; empty when the title reads the pad another way.
  std::optional<psx::input::StockPadBuffers::Layout> padBuffers;
};

// Compile-time check each title's facts file asserts: ranges are non-empty and the VSync seams sit inside GpuUpdate.
constexpr bool isCoherent(const TitleFacts &facts) {
  const FrameProgram &frame = facts.frame;
  return facts.libcdInit.initialize.valid() && facts.callbackBoot.initialize.valid() &&
         (!facts.gpuWatchdog || (facts.gpuWatchdog->start.valid() && facts.gpuWatchdog->check.valid())) &&
         facts.vsync.valid() && facts.vsyncQueryCounter != 0 && frame.coreLoop.valid() && frame.iteration.valid() &&
         frame.transition.valid() && frame.gpuUpdate.valid() && frame.coreLoop.contains(frame.iteration.begin) &&
         frame.gpuUpdate.contains(frame.firstVSync) && frame.gpuUpdate.contains(frame.afterFirstVSync) &&
         frame.gpuUpdate.contains(frame.secondVSync) && frame.gpuUpdate.contains(frame.afterVSync) &&
         (!frame.limiter || frame.limiter->skipAfterFields != 0);
}

} // namespace crash
