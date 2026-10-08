#include "crash1_frame_cut.h"

#include "core.h"

#include <lucent/log.h>

#ifndef CRASH1_SCENE_ID_ADDRESS
#error "CRASH1 scene words must come from titles/crash1/executable.json"
#endif

namespace crash1 {
namespace {

constexpr std::uint32_t kSceneIdAddress = CRASH1_SCENE_ID_ADDRESS;
constexpr std::uint32_t kSceneRequestAddress = CRASH1_SCENE_REQUEST_ADDRESS;
// CoreLoop 0x80011FC4 treats -1 as no request; a scene id or -2 (checkpoint reload) is a pending load.
constexpr std::uint32_t kNoRequest = 0xFFFFFFFFu;

} // namespace

void Crash1FrameCut::observe(Core &core) {
  const std::uint32_t scene = core.mem_r32(kSceneIdAddress);
  // The request is consumed at the next iteration's top, so the frame after a pending request is the cut.
  cut_ = !scene_.has_value() || *scene_ != scene || requestPending_;
  scene_ = scene;
  const std::uint32_t request = core.mem_r32(kSceneRequestAddress);
  requestPending_ = request != kNoRequest;
  if (cut_) {
    lucent::debug("cut", "cut scene={:#x} request={:#x}", scene, request);
  }
}

} // namespace crash1
