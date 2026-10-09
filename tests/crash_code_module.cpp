#include "cd_stock_read_completion.h"
#include "core.h"
#include "crash_titles.h"
#include "game.h"
#include "image_identity.h"

#include <cstdio>
#include <cstring>
#include <memory>

// Crash 2 reads level code into its heap and calls it; Crash 1 streams only data.
int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    const bool loadsCode = !facts.codeModuleArena.empty();
    const std::uint32_t inside = 0x800D8218u;
    const std::uint32_t outside = 0x80020000u;
    std::memset(core.ram + (inside & 0x1FFFFFFFu), 0x11, 2048u);
    std::memset(core.ram + (outside & 0x1FFFFFFFu), 0x22, 2048u);
    int bad = 0;

    game->runtime->stockCdReadLanded(core, {.firstLba = 56488u, .sectors = 1u, .destination = inside, .bytes = 2048u});
    if (core.currentImageIdentity(inside + 0x5A4u).has_value() != loadsCode) {
      bad += crash_test::fail(title, "a heap read must become a code image exactly when the title loads code");
    }
    game->runtime->stockCdReadLanded(core, {.firstLba = 24u, .sectors = 1u, .destination = outside, .bytes = 2048u});
    if (core.currentImageIdentity(outside).has_value()) {
      bad += crash_test::fail(title, "a read outside the loader's arena must publish nothing");
    }
    return bad;
  });
  std::printf("code module landing: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
