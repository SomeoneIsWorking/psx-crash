#pragma once

#include "boot/leaf_call.h"
#include "runtime/guest_function_range.h"

#include <cstdint>

namespace crash::gpu_watchdog {

// The libgpu queue timeout bookkeeping one title measured: both entries and the driver state they touch.
struct Program {
  GuestFunctionRange start;
  GuestFunctionRange check;
  std::uint32_t deadline;
  std::uint32_t pollCount;
  std::uint32_t queueRead;
  std::uint32_t queueWrite;
  std::uint32_t gp0Pointer;
  std::uint32_t statusPointer;
  std::uint32_t gp1Pointer;
  std::uint32_t dmaControlPointer;
  std::uint32_t lastCallback;
  std::uint32_t lastPayload;
  std::uint32_t lastArgument;
  std::uint32_t criticalToken;
  std::uint32_t diagnosticFormat;
  std::uint32_t callbackFormat;
  std::uint32_t log;
  std::uint32_t critical;
};

void registerOverrides(Core &core, const Program &program);

// GPU queue timeout bookkeeping on the host display counter.
void start(Core &core, const Program &program);
void check(Core &core, const Program &program, LeafDispatch dispatch);

} // namespace crash::gpu_watchdog
