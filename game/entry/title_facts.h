#pragma once

#include "callback_boot.h"
#include "frame_program.h"
#include "gpu_watchdog.h"
#include "guest_pad_buffer_layout.h"
#include "guest_program_image.h"
#include "libcd_init.h"
#include "native_frame_loop_contract.h"
#include "stock_libcd.h"

#include <cstdint>

class Core;

namespace crash {

// Everything one title's native boot and frame loop need from its executable, measured in
// `titles/<title>/executable.json` and the decompilation each title's facts file cites.
struct TitleFacts {
  const char *discEnvVar;
  GuestProgramImage image;
  // The retail C main's three steps before the main loop: static constructors, the CD flag, Init.
  std::uint32_t staticConstructors;
  std::uint32_t useCdAddress;
  std::uint32_t init;
  libcd_init::Program libcdInit;
  stock_libcd::Program stockLibcd;
  callback_boot::Program callbackBoot;
  gpu_watchdog::Program gpuWatchdog;
  FrameProgram frame;
  NativeFrameLoopContract contract;
  // Physical range the title's loader streams code modules into through stock CdRead; empty when it loads none.
  GuestAddressRange codeModuleArena;
  // Null when the title reads the pad through a word it publishes itself (see `CrashFrameDriver::publishInput`).
  const GuestPadBufferLayout *padBuffers;
};

// The facts of the title running on `core`; a core with no Crash runtime stops the run.
const TitleFacts &titleFacts(const Core &core);

} // namespace crash
