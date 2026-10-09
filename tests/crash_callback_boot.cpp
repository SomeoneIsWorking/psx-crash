#include "callback_boot.h"
#include "core.h"
#include "crash_titles.h"
#include "game.h"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

struct Call {
  std::uint32_t address;
  std::uint32_t arg0;
  std::uint32_t continuation;
};

std::vector<Call> gCalls;
std::uint32_t gNextHandle = 0x100u;
std::uint32_t gOpenEvent = 0;

void recordDispatch(Core *core, std::uint32_t address) {
  gCalls.push_back({address, core->r[4], core->r[31]});
  if (address == gOpenEvent) {
    core->r[2] = gNextHandle++;
  }
}

} // namespace

int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    const crash::callback_boot::Program &p = facts.callbackBoot;
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    int bad = 0;
    gCalls.clear();
    gNextHandle = 0x100u;
    gOpenEvent = p.openEvent;
    core.r[28] = 0x80056000u;
    core.r[29] = 0x801FFF00u;
    core.r[31] = 0x81234560u;

    crash::callback_boot::initializeDriver(core, p, recordDispatch);

    if (gCalls.size() != 21u) {
      return crash_test::fail(title, "callback initializer did not preserve all 21 state-producing calls");
    }
    if (gCalls[0].address != p.enterCritical || gCalls[9].address != p.exitCritical) {
      bad += crash_test::fail(title, "callback initializer critical section is not enter, 8 opens, exit");
    }
    if (gCalls[10].address != p.initializePad || gCalls[10].arg0 != 1u || gCalls[11].address != p.startPad ||
        gCalls[12].address != p.changeClearPad) {
      bad += crash_test::fail(title, "callback initializer pad service calls are wrong");
    }
    for (std::size_t index = 0; index < p.handleOffsets.size(); ++index) {
      if (core.mem_r32(core.r[28] + p.handleOffsets[index]) != 0x100u + index) {
        bad += crash_test::fail(title, "OpenEvent result was not stored at its gp-relative slot");
      }
      if (gCalls[13u + index].address != p.closeEvent || gCalls[13u + index].arg0 != 0x100u + index) {
        bad += crash_test::fail(title, "CloseEvent did not receive the matching OpenEvent handle");
      }
    }
    // Every continuation must be an address inside the retail initializer body.
    for (const Call &call : gCalls) {
      if (!p.initialize.contains(call.continuation)) {
        bad += crash_test::fail(title, "a callback leaf returns outside the retail initializer");
      }
    }
    if (core.r[29] != 0x801FFF00u || core.r[31] != 0x81234560u) {
      bad += crash_test::fail(title, "callback initializer did not restore stack and return address");
    }
    return bad;
  });
  std::printf("callback boot owner: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
