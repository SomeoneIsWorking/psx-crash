#include "crash1_disc_index_io.h"

#include "cd_control.h"
#include "core.h"
#include "native_dispatch.h"

#include <array>
#include <lucent/log.h>

#ifndef CRASH1_DISC_INDEX_ENTRY
#error "Crash 1 disc-index facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_DISC_INDEX_END
#error "Crash 1 disc-index facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_CONTROL_ENTRY
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_CONTROL_END
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_CONTROL_F_ENTRY
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_CONTROL_F_END
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_SYNC_WRAPPER_ENTRY
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_SYNC_WRAPPER_END
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_SYNC_ENTRY
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_SYNC_END
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_READ_ENTRY
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_READ_END
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_READ_SYNC_ENTRY
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_READ_SYNC_END
#error "Crash 1 stock-libcd facts must come from titles/crash1/executable.json"
#endif

namespace crash1::disc_index_io {
namespace {

constexpr Program kProgram{
    .reader = {CRASH1_DISC_INDEX_ENTRY, CRASH1_DISC_INDEX_END},
    .control = {CRASH1_CD_CONTROL_ENTRY, CRASH1_CD_CONTROL_END},
    .controlF = {CRASH1_CD_CONTROL_F_ENTRY, CRASH1_CD_CONTROL_F_END},
    .syncWrapper = {CRASH1_CD_SYNC_WRAPPER_ENTRY, CRASH1_CD_SYNC_WRAPPER_END},
    .sync = {CRASH1_CD_SYNC_ENTRY, CRASH1_CD_SYNC_END},
    .read = {CRASH1_CD_READ_ENTRY, CRASH1_CD_READ_END},
    .readSync = {CRASH1_CD_READ_SYNC_ENTRY, CRASH1_CD_READ_SYNC_END},
};

struct Binding {
  crash::GuestFunctionRange function;
  psx::cpu::NativeFunction owner;
  const char *name;
};

constexpr std::array kBindings{
    Binding{kProgram.control, applyControl, "CdControl"},
    Binding{kProgram.controlF, applyControlF, "CdControlF"},
    Binding{kProgram.syncWrapper, applySync, "CdSync wrapper"},
    Binding{kProgram.sync, applySync, "CdSync body"},
    Binding{kProgram.read, applyRead, "CdRead"},
    Binding{kProgram.readSync, applyReadSync, "CdReadSync"},
};

static_assert(kProgram.reader.valid());
static_assert(kProgram.control.valid());
static_assert(kProgram.controlF.valid());
static_assert(kProgram.syncWrapper.valid());
static_assert(kProgram.sync.valid());
static_assert(kProgram.read.valid());
static_assert(kProgram.readSync.valid());

} // namespace

const Program &program() {
  return kProgram;
}

void applyControl(Core *core) {
  cd_control_sync(core);
}

void applyControlF(Core *core) {
  // CdControlF has no result-pointer argument; zero a2 so a stale register is not taken as guest memory.
  const std::uint32_t callerA2 = core->r[6];
  core->r[6] = 0u;
  cd_control_sync(core);
  core->r[6] = callerA2;
}

void applySync(Core *core) {
  cd_sync_stock_sync(core);
}

void applyRead(Core *core) {
  cd_read_stock_sync(core);
}

void applyReadSync(Core *core) {
  cd_readsync_stock_sync(core);
}

void registerOverrides(Core &core) {
  for (const Binding &binding : kBindings) {
    psx::cpu::installNativeOverride(core, binding.function.begin, binding.name, binding.owner);
  }
}

} // namespace crash1::disc_index_io
