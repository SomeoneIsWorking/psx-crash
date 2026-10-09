#pragma once

#include "native_frame_loop_contract.h"

class Core;

namespace crash::stock_libcd {

// The libcd entries a title measured; each is bound to the framework's synchronous disc owner.
struct Program {
  GuestFunctionRange control;
  GuestFunctionRange controlF;
  GuestFunctionRange syncWrapper;
  GuestFunctionRange sync;
  GuestFunctionRange read;
  GuestFunctionRange readSync;
};

void registerOverrides(Core &core, const Program &program);

void applyControl(Core *core);
void applyControlF(Core *core);
void applySync(Core *core);
void applyRead(Core *core);
void applyReadSync(Core *core);

} // namespace crash::stock_libcd
