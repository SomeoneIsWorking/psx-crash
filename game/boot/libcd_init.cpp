#include "libcd_init.h"

#include "core.h"
#include "native_dispatch.h"
#include "title_facts.h"

namespace crash::libcd_init {
namespace {

constexpr std::uint8_t kCompleteStatus = 2u;

void initializeOverride(Core *core) {
  initializeDriver(*core, titleFacts(*core).libcdInit);
}

} // namespace

void registerOverride(Core &core, const Program &program) {
  psx::cpu::installNativeOverride(core, program.initialize.begin, "libcd initialization", initializeOverride);
}

void initializeDriver(Core &core, const Program &program) {
  // libcd software-state init; its hardware leg waits on VSync(-1)-timed CdSync.
  // Only the guest-visible library state is reproduced; Game owns the native CdcState.
  core.mem_w32(program.lastSyncCallback, 0u);
  core.mem_w32(program.lastReadyCallback, 0u);
  core.mem_w32(program.lastStatus, 0u);
  core.mem_w32(program.lastResult, 0u);
  core.mem_w8(program.lastCommand, 0u);
  core.mem_w8(program.pendingCommand, 0u);
  for (std::uint32_t word = 0; word < program.commandWorkspaceWords; ++word) {
    core.mem_w32(program.commandWorkspace + word * sizeof(std::uint32_t), 0u);
  }
  core.mem_w8(program.syncStatus, kCompleteStatus);
  core.mem_w8(program.syncStatus + 1u, 0u);
  core.mem_w8(program.syncStatus + 2u, 0u);
  core.r[2] = 0u;
}

} // namespace crash::libcd_init
