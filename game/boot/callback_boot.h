#pragma once

#include "boot/leaf_call.h"
#include "runtime/guest_function_range.h"

#include <array>
#include <cstdint>

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

// Native owner over the retail callback/event initializer; the Lightrec body stays reachable via callOriginal.
void registerOverride(Core &core, const Program &program);

// The main-loop entry is injected, so each guest call can name its continuation.
void initializeDriver(Core &core, const Program &program, LeafDispatch dispatch);

} // namespace crash::callback_boot
