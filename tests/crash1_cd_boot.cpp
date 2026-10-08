#include "crash1_cd_boot.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "game_runtime.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

constexpr std::uint32_t kLastSyncCallback = 0x800555A0u;
constexpr std::uint32_t kLastReadyCallback = 0x800555A4u;
constexpr std::uint32_t kLastStatus = 0x800555B0u;
constexpr std::uint32_t kLastResult = 0x800555B4u;
constexpr std::uint32_t kLastCommand = 0x800555C0u;
constexpr std::uint32_t kPendingCommand = 0x800555C1u;
constexpr std::uint32_t kCommandWorkspace = 0x80055880u;
constexpr std::uint32_t kSyncStatus = 0x8005587Cu;
constexpr std::uint32_t kReadyStatus = 0x8005587Du;
constexpr std::uint32_t kSecondaryStatus = 0x8005587Eu;

int gFailures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", detail);
    ++gFailures;
  }
}

} // namespace

int main() {
  static crash1::Crash1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  core.mem_w32(kLastSyncCallback, 0x11111111u);
  core.mem_w32(kLastReadyCallback, 0x22222222u);
  core.mem_w32(kLastStatus, 0x33333333u);
  core.mem_w32(kLastResult, 0x44444444u);
  core.mem_w8(kLastCommand, 0x55u);
  core.mem_w8(kPendingCommand, 0x66u);
  for (std::uint32_t word = 0; word < 10u; ++word) {
    core.mem_w32(kCommandWorkspace + word * sizeof(std::uint32_t), 0xA5A50000u + word);
  }
  core.mem_w8(kSyncStatus, 0x77u);
  core.mem_w8(kReadyStatus, 0x88u);
  core.mem_w8(kSecondaryStatus, 0x99u);
  core.r[2] = 0xFFFFFFFFu;

  crash1::cd_boot::initializeDriver(core);

  check(core.mem_r32(kLastSyncCallback) == 0u, "sync callback was not cleared");
  check(core.mem_r32(kLastReadyCallback) == 0u, "ready callback was not cleared");
  check(core.mem_r32(kLastStatus) == 0u, "last status was not cleared");
  check(core.mem_r32(kLastResult) == 0u, "last result was not cleared");
  check(core.mem_r8(kLastCommand) == 0u, "last command was not cleared");
  check(core.mem_r8(kPendingCommand) == 0u, "pending command was not cleared");
  for (std::uint32_t word = 0; word < 10u; ++word) {
    check(core.mem_r32(kCommandWorkspace + word * sizeof(std::uint32_t)) == 0u,
          "command workspace word was not cleared");
  }
  check(core.mem_r8(kSyncStatus) == 2u, "sync status was not marked complete");
  check(core.mem_r8(kReadyStatus) == 0u, "ready status was not cleared");
  check(core.mem_r8(kSecondaryStatus) == 0u, "secondary status was not cleared");
  check(core.r[2] == 0u, "native libcd initialization did not report success");
  check(game->cdc.stat == 2u, "native CD controller power-on state was disturbed");

  std::printf("crash1 CD boot owner: %s\n", gFailures == 0 ? "PASS" : "FAIL");
  return gFailures == 0 ? 0 : 1;
}
