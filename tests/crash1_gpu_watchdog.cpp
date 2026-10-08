#include "crash1_gpu_watchdog.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "game_runtime.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

constexpr std::uint32_t kDeadline = 0x80054B84u;
constexpr std::uint32_t kPollCount = 0x80054B88u;

std::vector<std::uint32_t> gCalls;
int gFailures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", detail);
    ++gFailures;
  }
}

void recordDispatch(Core *core, std::uint32_t address) {
  gCalls.push_back(address);
  if (address == 0x8003E870u) {
    core->r[2] = 0x1234u;
  }
}

} // namespace

int main() {
  static crash1::Crash1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  game->timing.vblank = 17u;

  crash1::gpu_watchdog::start(core);
  check(core.mem_r32(kDeadline) == 257u, "GPU watchdog deadline did not use the native field counter");
  check(core.mem_r32(kPollCount) == 0u, "GPU watchdog poll count was not reset");

  core.r[2] = 0xFFFFFFFFu;
  crash1::gpu_watchdog::check(core, recordDispatch);
  check(core.r[2] == 0u, "healthy GPU watchdog check did not report success");
  check(core.mem_r32(kPollCount) == 1u, "GPU watchdog check did not count its poll");
  check(gCalls.empty(), "healthy GPU watchdog check dispatched recovery work");

  std::printf("crash1 GPU watchdog owner: %s\n", gFailures == 0 ? "PASS" : "FAIL");
  return gFailures == 0 ? 0 : 1;
}
