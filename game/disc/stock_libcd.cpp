#include "stock_libcd.h"

#include "cd_control.h"
#include "core.h"
#include "native_dispatch.h"

#include <array>

namespace crash::stock_libcd {
namespace {

struct Binding {
  GuestFunctionRange function;
  psx::cpu::NativeFunction owner;
  const char *name;
};

} // namespace

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

void registerOverrides(Core &core, const Program &program) {
  const std::array bindings{
      Binding{program.control, applyControl, "CdControl"},
      Binding{program.controlF, applyControlF, "CdControlF"},
      Binding{program.syncWrapper, applySync, "CdSync wrapper"},
      Binding{program.sync, applySync, "CdSync body"},
      Binding{program.read, applyRead, "CdRead"},
      Binding{program.readSync, applyReadSync, "CdReadSync"},
  };
  for (const Binding &binding : bindings) {
    psx::cpu::installNativeOverride(core, binding.function.begin, binding.name, binding.owner);
  }
}

} // namespace crash::stock_libcd
