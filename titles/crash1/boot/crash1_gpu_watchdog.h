#pragma once

#include "native_frame_loop_contract.h"

#include <cstdint>

struct Core;

namespace crash1::gpu_watchdog {

struct Program {
  crash::GuestFunctionRange start;
  crash::GuestFunctionRange check;
};

using MainDispatch = void (*)(Core *, std::uint32_t);

const Program &program();
void registerOverrides(Core &core);

// GPU queue timeout bookkeeping on the host display counter.
void start(Core &core);
void check(Core &core, MainDispatch dispatch);

} // namespace crash1::gpu_watchdog
