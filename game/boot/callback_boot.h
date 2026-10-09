#pragma once

#include "native_frame_loop_contract.h"

#include <array>
#include <cstdint>

class Core;

namespace crash::callback_boot {

inline constexpr std::size_t kEventCount = 8;

// The retail initializer's guest calls. The body is word-identical across the trilogy; a title supplies
// the leaf addresses and the gp-relative slots its handles are stored in.
struct Program {
  GuestFunctionRange initialize;
  std::uint32_t enterCritical;
  std::uint32_t openEvent;
  std::uint32_t exitCritical;
  std::uint32_t closeEvent;
  std::uint32_t initializePad;
  std::uint32_t startPad;
  std::uint32_t changeClearPad;
  std::array<std::uint32_t, kEventCount> handleOffsets;
};

using MainDispatch = void (*)(Core *, std::uint32_t);

// Native owner over the retail callback/event initializer; the Lightrec body stays reachable via callOriginal.
void registerOverride(Core &core, const Program &program);

// The main-loop entry is injected, so each guest call can name its continuation.
void initializeDriver(Core &core, const Program &program, MainDispatch dispatch);

} // namespace crash::callback_boot
