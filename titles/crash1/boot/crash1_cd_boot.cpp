#include "crash1_cd_boot.h"

#include "core.h"
#include "native_dispatch.h"

#include <lucent/log.h>

#ifndef CRASH1_CD_INITIALIZE_ENTRY
#error "CRASH1 CD initialization facts must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CD_INITIALIZE_END
#error "CRASH1 CD initialization facts must come from titles/crash1/executable.json"
#endif

namespace crash1::cd_boot {
namespace {

constexpr Program kProgram{
    .initialize = {CRASH1_CD_INITIALIZE_ENTRY, CRASH1_CD_INITIALIZE_END},
};

constexpr std::uint32_t kLastSyncCallback = 0x800555A0u;
constexpr std::uint32_t kLastReadyCallback = 0x800555A4u;
constexpr std::uint32_t kLastCommand = 0x800555C0u;
constexpr std::uint32_t kPendingCommand = 0x800555C1u;
constexpr std::uint32_t kLastStatus = 0x800555B0u;
constexpr std::uint32_t kLastResult = 0x800555B4u;
constexpr std::uint32_t kCommandWorkspace = 0x80055880u;
constexpr std::uint32_t kCommandWorkspaceWords = 10u;
constexpr std::uint32_t kSyncStatus = 0x8005587Cu;
constexpr std::uint32_t kReadyStatus = 0x8005587Du;
constexpr std::uint32_t kSecondaryStatus = 0x8005587Eu;
constexpr std::uint8_t kCompleteStatus = 2u;

static_assert(kProgram.initialize.valid());

void initializeOverride(Core *core) {
  initializeDriver(*core);
}

} // namespace

const Program &program() {
  return kProgram;
}

void registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kProgram.initialize.begin, "Crash 1 libcd initialization", initializeOverride);
}

void initializeDriver(Core &core) {
  // libcd software-state init [0x80044E8C,0x8004519C); its hardware leg waits on VSync(-1)-timed CdSync.
  // Only the guest-visible library state is reproduced; Game owns the native CdcState.
  core.mem_w32(kLastSyncCallback, 0u);
  core.mem_w32(kLastReadyCallback, 0u);
  core.mem_w32(kLastStatus, 0u);
  core.mem_w32(kLastResult, 0u);
  core.mem_w8(kLastCommand, 0u);
  core.mem_w8(kPendingCommand, 0u);
  for (std::uint32_t word = 0; word < kCommandWorkspaceWords; ++word) {
    core.mem_w32(kCommandWorkspace + word * sizeof(std::uint32_t), 0u);
  }
  core.mem_w8(kSyncStatus, kCompleteStatus);
  core.mem_w8(kReadyStatus, 0u);
  core.mem_w8(kSecondaryStatus, 0u);
  core.r[2] = 0u;
}

} // namespace crash1::cd_boot
