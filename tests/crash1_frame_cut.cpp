#include "crash1_frame_cut.h"
#include "core.h"
#include "game.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

constexpr std::uint32_t kSceneId = 0x80056710u;
constexpr std::uint32_t kSceneRequest = 0x80056714u;
constexpr std::uint32_t kNoRequest = 0xFFFFFFFFu;

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
  }
  return condition;
}

void frame(Core &core, crash1::Crash1FrameCut &cut, std::uint32_t scene, std::uint32_t request) {
  core.mem_w32(kSceneId, scene);
  core.mem_w32(kSceneRequest, request);
  cut.observe(core);
}

} // namespace

int main() {
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  crash1::Crash1FrameCut cut;
  bool ok = true;

  frame(core, cut, 0x19u, kNoRequest);
  ok = expect(cut.isCut(), "a session's first sealed frame is a cut") && ok;
  frame(core, cut, 0x19u, kNoRequest);
  ok = expect(!cut.isCut(), "a frame in the same scene with no request is continuous") && ok;

  frame(core, cut, 0x19u, 0x00u);
  ok = expect(!cut.isCut(), "the frame that raises a scene request is still the old scene") && ok;
  frame(core, cut, 0x00u, kNoRequest);
  ok = expect(cut.isCut(), "the frame after a request loads the new scene") && ok;
  frame(core, cut, 0x00u, kNoRequest);
  ok = expect(!cut.isCut(), "the frame after the load is continuous") && ok;

  frame(core, cut, 0x00u, 0xFFFFFFFEu);
  frame(core, cut, 0x00u, kNoRequest);
  ok = expect(cut.isCut(), "a checkpoint reload of the same scene id (-2) is a cut") && ok;
  frame(core, cut, 0x00u, kNoRequest);
  ok = expect(!cut.isCut(), "the frame after the reload is continuous") && ok;

  frame(core, cut, 0x19u, kNoRequest);
  ok = expect(cut.isCut(), "a scene id change with no observed request is a cut") && ok;

  if (ok) {
    std::printf("Crash1FrameCut: scene requests and scene changes are cuts\n");
  }
  return ok ? 0 : 1;
}
