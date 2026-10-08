#pragma once

#include "native_frame_loop_contract.h"

struct Core;

namespace crash1::disc_index_io {

struct Program {
  crash::GuestFunctionRange reader;
  crash::GuestFunctionRange control;
  crash::GuestFunctionRange controlF;
  crash::GuestFunctionRange syncWrapper;
  crash::GuestFunctionRange sync;
  crash::GuestFunctionRange read;
  crash::GuestFunctionRange readSync;
};

const Program &program();

// Bind Crash's measured stock-libcd wrappers to the framework's synchronous command/data owners.
void registerOverrides(Core &core);

void applyControl(Core *core);
void applyControlF(Core *core);
void applySync(Core *core);
void applyRead(Core *core);
void applyReadSync(Core *core);

} // namespace crash1::disc_index_io
