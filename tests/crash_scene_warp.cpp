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

    if (!contains(warp.arm("warp", true), "usage") || !contains(warp.arm("warp 3D", true), "refused") ||
        !contains(warp.arm("warp 15", false), "refused")) {
      bad += crash_test::fail(title, "a malformed, out-of-range or early warp must be refused");
    }
    core.mem_w32(program.sceneRequestAddress, crash::SceneWarp::kNoRequest);
    if (warp.apply(core, program).has_value()) {
      bad += crash_test::fail(title, "an unarmed warp must apply nothing");
    }

    if (!contains(warp.arm("warp 15", true), "ok")) {
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
