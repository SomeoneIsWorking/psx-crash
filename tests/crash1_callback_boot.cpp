#include "crash1_callback_boot.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "game_runtime.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

struct Call {
  std::uint32_t address;
  std::array<std::uint32_t, 4> args;
  std::uint32_t continuation;
};

constexpr std::uint32_t kOpenEvent = 0x8003E1A8u;
constexpr std::uint32_t kVSync = 0x8003E4F0u;
constexpr std::array<std::uint32_t, 8> kHandleOffsets{712u, 716u, 720u, 724u, 752u, 756u, 764u, 768u};

std::vector<Call> gCalls;
std::uint32_t gNextHandle = 0x100u;
int gFailures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", detail);
    ++gFailures;
  }
}

void recordDispatch(Core *core, std::uint32_t address) {
  gCalls.push_back({address, {core->r[4], core->r[5], core->r[6], core->r[7]}, core->r[31]});
  if (address == kOpenEvent) {
    core->r[2] = gNextHandle++;
  }
}

} // namespace

int main() {
  static crash1::Crash1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.r[28] = 0x80056000u;
  core.r[29] = 0x801FFF00u;
  core.r[31] = 0x81234560u;

  crash1::callback_boot::initializeDriver(core, recordDispatch);

  check(gCalls.size() == 21u, "callback initializer did not preserve all 21 state-producing calls");
  for (const Call &call : gCalls) {
    check(call.address != kVSync, "callback initializer dispatched guest VSync");
  }
  check(gCalls[0].address == 0x8003E1F8u, "callback initializer did not enter its critical section first");
  check(gCalls[9].address == 0x8003E208u, "callback initializer did not exit its critical section");
  check(gCalls[10].address == 0x80051444u && gCalls[10].args[0] == 1u,
        "callback initializer did not initialize the pad with the retail argument");
  check(gCalls[11].address == 0x80051454u, "callback initializer did not start the pad");
  check(gCalls[12].address == 0x8003E198u, "callback initializer did not preserve ChangeClearPad");
  for (std::size_t index = 0; index < kHandleOffsets.size(); ++index) {
    check(core.mem_r32(core.r[28] + kHandleOffsets[index]) == 0x100u + index,
          "OpenEvent result was not stored at its measured gp-relative location");
    check(gCalls[13u + index].address == 0x8003E1D8u, "callback initializer did not close a temporary event handle");
    check(gCalls[13u + index].args[0] == 0x100u + index,
          "CloseEvent did not receive the corresponding OpenEvent handle");
  }
  check(core.r[29] == 0x801FFF00u, "callback initializer did not restore the guest stack pointer");
  check(core.r[31] == 0x81234560u, "callback initializer did not restore the guest return address");

  std::printf("crash1 callback boot owner: %s\n", gFailures == 0 ? "PASS" : "FAIL");
  return gFailures == 0 ? 0 : 1;
}
