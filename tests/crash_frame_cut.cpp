#include "core.h"
#include "crash_titles.h"
#include "frame_cut.h"
#include "game.h"

#include <cstdio>
#include <memory>

namespace {

constexpr std::uint32_t kNoRequest = 0xFFFFFFFFu;

bool expect(const char *title, bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL %s: %s\n", title, message);
  }
  return condition;
}

} // namespace

int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    crash::FrameCut cut(facts.frame.sceneIdAddress, facts.frame.sceneRequestAddress);
    const auto frame = [&](std::uint32_t scene, std::uint32_t request) {
      core.mem_w32(facts.frame.sceneIdAddress, scene);
      core.mem_w32(facts.frame.sceneRequestAddress, request);
      cut.observe(core);
    };
    bool ok = true;

    frame(0x19u, kNoRequest);
    ok = expect(title, cut.isCut(), "a session's first sealed frame is a cut") && ok;
    frame(0x19u, kNoRequest);
    ok = expect(title, !cut.isCut(), "a frame in the same scene with no request is continuous") && ok;
    frame(0x19u, 0x00u);
    ok = expect(title, !cut.isCut(), "the frame that raises a scene request is still the old scene") && ok;
    frame(0x00u, kNoRequest);
    ok = expect(title, cut.isCut(), "the frame after a request loads the new scene") && ok;
    frame(0x00u, kNoRequest);
    ok = expect(title, !cut.isCut(), "the frame after the load is continuous") && ok;
    frame(0x00u, 0xFFFFFFFEu);
    frame(0x00u, kNoRequest);
    ok = expect(title, cut.isCut(), "a checkpoint reload of the same scene id (-2) is a cut") && ok;
    frame(0x19u, kNoRequest);
    ok = expect(title, cut.isCut(), "a scene id change with no observed request is a cut") && ok;
    cut.reset();
    ok = expect(title, cut.isCut(), "reset starts the next session with a cut") && ok;
    return ok ? 0 : 1;
  });
  std::printf("FrameCut: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
