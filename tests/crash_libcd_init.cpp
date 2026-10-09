#include "core.h"
#include "crash_titles.h"
#include "game.h"
#include "libcd_init.h"

#include <cstdio>
#include <memory>

int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    const crash::libcd_init::Program &p = facts.libcdInit;
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    int bad = 0;
    core.mem_w32(p.lastSyncCallback, 0x11111111u);
    core.mem_w32(p.lastReadyCallback, 0x22222222u);
    core.mem_w32(p.lastStatus, 0x33333333u);
    core.mem_w32(p.lastResult, 0x44444444u);
    core.mem_w8(p.lastCommand, 0x55u);
    core.mem_w8(p.pendingCommand, 0x66u);
    for (std::uint32_t word = 0; word < p.commandWorkspaceWords; ++word) {
      core.mem_w32(p.commandWorkspace + word * 4u, 0xA5A50000u + word);
    }
    core.mem_w8(p.syncStatus, 0x77u);
    core.mem_w8(p.syncStatus + 1u, 0x88u);
    core.mem_w8(p.syncStatus + 2u, 0x99u);
    core.r[2] = 0xFFFFFFFFu;

    crash::libcd_init::initializeDriver(core, p);

    if (core.mem_r32(p.lastSyncCallback) != 0u || core.mem_r32(p.lastReadyCallback) != 0u ||
        core.mem_r32(p.lastStatus) != 0u || core.mem_r32(p.lastResult) != 0u) {
      bad += crash_test::fail(title, "libcd callbacks or status were not cleared");
    }
    if (core.mem_r8(p.lastCommand) != 0u || core.mem_r8(p.pendingCommand) != 0u) {
      bad += crash_test::fail(title, "libcd command bytes were not cleared");
    }
    for (std::uint32_t word = 0; word < p.commandWorkspaceWords; ++word) {
      if (core.mem_r32(p.commandWorkspace + word * 4u) != 0u) {
        bad += crash_test::fail(title, "command workspace word was not cleared");
      }
    }
    if (core.mem_r8(p.syncStatus) != 2u || core.mem_r8(p.syncStatus + 1u) != 0u ||
        core.mem_r8(p.syncStatus + 2u) != 0u) {
      bad += crash_test::fail(title, "sync status bytes are not complete/0/0");
    }
    if (core.r[2] != 0u) {
      bad += crash_test::fail(title, "libcd initialization did not report success");
    }
    if (game->cdc.stat != 2u) {
      bad += crash_test::fail(title, "native CD controller power-on state was disturbed");
    }
    return bad;
  });
  std::printf("libcd initialization owner: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
