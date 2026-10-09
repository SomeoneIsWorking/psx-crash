#include "frame_cut.h"

#include "core.h"

#include <lucent/log.h>

namespace crash {
namespace {

// CoreLoop treats -1 as no request; a scene id or -2 (checkpoint reload) is a pending load.
constexpr std::uint32_t kNoRequest = 0xFFFFFFFFu;

} // namespace

FrameCut::FrameCut(std::uint32_t sceneIdAddress, std::uint32_t sceneRequestAddress)
    : sceneIdAddress_(sceneIdAddress), sceneRequestAddress_(sceneRequestAddress) {}

void FrameCut::reset() {
  scene_.reset();
  requestPending_ = false;
  cut_ = true;
}

void FrameCut::observe(Core &core) {
  const std::uint32_t scene = core.mem_r32(sceneIdAddress_);
  // The request is consumed at the next iteration's top, so the frame after a pending request is the cut.
  cut_ = !scene_.has_value() || *scene_ != scene || requestPending_;
  scene_ = scene;
  const std::uint32_t request = core.mem_r32(sceneRequestAddress_);
  requestPending_ = request != kNoRequest;
  if (cut_) {
    lucent::debug("cut", "cut scene={:#x} request={:#x}", scene, request);
  }
}

} // namespace crash
