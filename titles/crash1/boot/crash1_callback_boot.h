#pragma once

#include "native_frame_loop_contract.h"

#include <cstdint>

struct Core;

namespace crash1::callback_boot {

struct Program {
  crash::GuestFunctionRange initialize;
};

using MainDispatch = void (*)(Core *, std::uint32_t);

const Program &program();

// Native owner over the retail callback/event initializer; the Lightrec body stays reachable via callOriginal.
void registerOverride(Core &core);

// The main-loop entry is injected, so each guest call can name its continuation.
void initializeDriver(Core &core, MainDispatch dispatch);

} // namespace crash1::callback_boot
