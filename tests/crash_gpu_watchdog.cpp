#include "core.h"
#include "crash_titles.h"
#include "game.h"
#include "gpu_watchdog.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace {

std::vector<std::uint32_t> gCalls;

void recordDispatch(Core *core, std::uint32_t address) {
  gCalls.push_back(address);
  core->r[2] = 0x1234u;
}

} // namespace

int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    const crash::gpu_watchdog::Program &p = facts.gpuWatchdog;
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    int bad = 0;
    gCalls.clear();
    game->timing.vblank = 17u;

    crash::gpu_watchdog::start(core, p);
    if (core.mem_r32(p.deadline) != 257u || core.mem_r32(p.pollCount) != 0u) {
      bad += crash_test::fail(title, "watchdog start did not arm a 240-field deadline and clear the poll count");
    }
    core.r[2] = 0xFFFFFFFFu;
    crash::gpu_watchdog::check(core, p, recordDispatch);
    if (core.r[2] != 0u || core.mem_r32(p.pollCount) != 1u || !gCalls.empty()) {
      bad += crash_test::fail(title, "a healthy check must count its poll, succeed and do no recovery");
    }

    // An expired deadline runs the retail recovery: log twice, then the critical-section pair.
    game->timing.vblank = 300u;
    core.r[29] = 0x801FFF00u;
    core.r[31] = 0x81234560u;
    core.mem_w32(p.gp0Pointer, 0x80060000u);
    core.mem_w32(p.gp1Pointer, 0x80060010u);
    core.mem_w32(p.statusPointer, 0x80060020u);
    core.mem_w32(p.dmaControlPointer, 0x80060030u);
    crash::gpu_watchdog::check(core, p, recordDispatch);
    if (core.r[2] != 0xFFFFFFFFu || gCalls.size() != 4u || gCalls[0] != p.log || gCalls[1] != p.log ||
        gCalls[2] != p.critical || gCalls[3] != p.critical) {
      bad += crash_test::fail(title, "an expired deadline did not run log, log, enter and exit critical section");
    }
    if (core.r[29] != 0x801FFF00u || core.r[31] != 0x81234560u) {
      bad += crash_test::fail(title, "recovery did not restore the guest stack and return address");
    }
    return bad;
  });
  std::printf("GPU watchdog owner: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
