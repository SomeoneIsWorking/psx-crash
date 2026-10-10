#include "core.h"
#include "crash_titles.h"
#include "game.h"
#include "scene_warp.h"

#include <cstdio>
#include <memory>
#include <string>

namespace {

bool contains(const std::string &text, const char *needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    const crash::FrameProgram &program = facts.frame;
    crash::SceneWarp warp;
    int bad = 0;

    if (!contains(warp.arm("warp", true, core, program), "usage") ||
        !contains(warp.arm("warp 3D", true, core, program), "refused") ||
        !contains(warp.arm("warp 15", false, core, program), "refused")) {
      bad += crash_test::fail(title, "a malformed, out-of-range or early warp must be refused");
    }
    core.mem_w32(program.sceneRequestAddress, crash::SceneWarp::kNoRequest);
    if (warp.apply(core, program).has_value()) {
      bad += crash_test::fail(title, "an unarmed warp must apply nothing");
    }

    const crash::SceneTable &table = program.sceneTable;
    if (table.address != 0u) {
      core.mem_w32(table.address + 0x15u * table.rowBytes + 4u, 0x1754u);
      core.mem_w32(table.address + 0x14u * table.rowBytes + 4u, 0u);
      core.mem_w32(table.address + 0x2Au * table.rowBytes + 4u, 0u);
      if (!contains(warp.arm("warp 14", true, core, program), "no row") ||
          !contains(warp.arm("warp 2A", true, core, program), "no row")) {
        bad += crash_test::fail(title, "a scene with an empty table row must be refused");
      }
      core.mem_w32(table.address + 0x16u * table.rowBytes + 4u, 0x2000000u);
      if (!contains(warp.arm("warp 16", true, core, program), "no row")) {
        bad += crash_test::fail(title, "a row whose size field masks to zero must be refused");
      }
    }
    if (!contains(warp.arm("warp 15", true, core, program), "ok")) {
      bad += crash_test::fail(title, "a valid warp must arm");
    }
    core.mem_w32(program.sceneRequestAddress, 0x2u);
    if (warp.apply(core, program).has_value() || core.mem_r32(program.sceneRequestAddress) != 0x2u) {
      bad += crash_test::fail(title, "a warp must wait while the guest has its own scene request pending");
    }
    core.mem_w32(program.sceneRequestAddress, crash::SceneWarp::kNoRequest);
    if (warp.apply(core, program) != std::optional<std::uint32_t>(0x15u) ||
        core.mem_r32(program.sceneRequestAddress) != 0x15u) {
      bad += crash_test::fail(title, "an armed warp must raise the guest's scene request");
    }
    if (warp.apply(core, program).has_value()) {
      bad += crash_test::fail(title, "a warp must apply once");
    }

    // Through the runtime's control surface, before the frame loop has run.
    std::FILE *out = std::tmpfile();
    const bool handled = game->runtime->controlCommand(core, "warp", "warp 15", out);
    std::rewind(out);
    char reply[128] = {};
    const bool read = std::fgets(reply, sizeof reply, out) != nullptr;
    std::fclose(out);
    if (!handled || !read || !contains(reply, "refused")) {
      bad += crash_test::fail(title, "the runtime must own `warp` and refuse it before the loop runs");
    }
    return bad;
  });
  std::printf("scene warp: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
