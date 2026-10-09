#pragma once

#include "native_frame_loop_contract.h"

#include <cstdint>

class Core;

namespace crash::libcd_init {

// Where one title's libcd keeps the software state its initializer resets.
struct Program {
  GuestFunctionRange initialize;
  std::uint32_t lastSyncCallback;
  std::uint32_t lastReadyCallback;
  std::uint32_t lastStatus;
  std::uint32_t lastResult;
  std::uint32_t lastCommand;
  std::uint32_t pendingCommand;
  // Zero words when the library keeps no command workspace.
  std::uint32_t commandWorkspace;
  std::uint32_t commandWorkspaceWords;
  // Three consecutive bytes: sync, ready, secondary.
  std::uint32_t syncStatus;
};

// Native owner over retail libcd initialization; the guest body stays reachable for A/B.
void registerOverride(Core &core, const Program &program);

void initializeDriver(Core &core, const Program &program);

} // namespace crash::libcd_init
