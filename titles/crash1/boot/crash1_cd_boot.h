#pragma once

#include "native_frame_loop_contract.h"

struct Core;

namespace crash1::cd_boot {

struct Program {
  crash::GuestFunctionRange initialize;
};

const Program &program();

// Native owner over retail libcd initialization; the guest body stays reachable for A/B.
void registerOverride(Core &core);

void initializeDriver(Core &core);

} // namespace crash1::cd_boot
