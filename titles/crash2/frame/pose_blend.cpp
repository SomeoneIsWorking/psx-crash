#include "crash2/frame/pose_blend.h"

#include "core.h"
#include "crash2/entry/session.h"
#include "native_dispatch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace crash2::pose_blend {
namespace {

// Names and layouts from external/crash2-decomp (src/game/8001A014.c, docs/notes/frame-loop.md).
constexpr std::uint32_t kFetchFrames = 0x8001A90Cu;
constexpr std::uint32_t kAnim = 0xE8u;
constexpr std::uint32_t kAnimFrame = 0xECu;
// A vertex frame's model entry id, which both frames of a morph must share.
constexpr std::uint32_t kFrameModel = 0x10u;

struct Fetched {
  std::uint32_t first = 0;
  std::uint32_t second = 0;
  std::uint32_t index = 0;
};

// The fetch's arguments: the object and where its two frames go.
struct Call {
  std::uint32_t object = 0;
  std::uint32_t outFirst = 0;
  std::uint32_t outSecond = 0;
};

struct Pose {
  std::uint32_t anim = 0;
  std::uint32_t frame = 0;
};

// The original fetch with the object's animation and frame set to `pose`.
Fetched fetch(Core &core, const Call &call, Pose pose) {
  core.mem_w32(call.object + kAnim, pose.anim);
  core.mem_w32(call.object + kAnimFrame, pose.frame);
  core.r[4] = call.object;
  core.r[5] = call.outFirst;
  core.r[6] = call.outSecond;
  psx::cpu::callOriginalToReturn(core, kFetchFrames, psx::cpu::ExecutionBudget::currentTurn(core), "Crash 2 pose");
  return {core.mem_r32(call.outFirst), core.mem_r32(call.outSecond), core.r[2]};
}

// The pair whose equal-weight sum is the four frames' mean, when there is one.
std::optional<std::pair<std::uint32_t, std::uint32_t>> halfway(const Fetched &from, const Fetched &to) {
  std::array<std::uint32_t, 4> frames{from.first, from.second, to.first, to.second};
  std::ranges::sort(frames);
  if (frames[0] != frames[1] || frames[2] != frames[3]) {
    return std::nullopt;
  }
  return std::pair{frames[0], frames[2]};
}

void fetchOverride(Core *core) {
  DrawReplay &replay = session(*core).drawReplay;
  const Call call{.object = core->r[4], .outFirst = core->r[5], .outSecond = core->r[6]};
  const std::optional<float> t = replay.frame().replaying();
  const std::optional<psx::present::MotionRule::Samples> samples =
      t && *t == 0.5f && call.outSecond != 0u ? replay.frame().motion().pairedRecord(replay.objectTrack(), call.object)
                                              : std::nullopt;
  if (!samples) {
    psx::cpu::callOriginalToReturn(*core, kFetchFrames, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 2 pose");
    return;
  }
  const Pose later{.anim = core->mem_r32(call.object + kAnim), .frame = core->mem_r32(call.object + kAnimFrame)};
  const Fetched from =
      fetch(*core,
            call,
            {.anim = samples->from[DrawReplay::kAnimField], .frame = samples->from[DrawReplay::kAnimFrameField]});
  const Fetched to = fetch(*core, call, later);
  if (const auto pair = halfway(from, to);
      pair && core->mem_r32(pair->first + kFrameModel) == core->mem_r32(pair->second + kFrameModel)) {
    core->mem_w32(call.outFirst, pair->first);
    core->mem_w32(call.outSecond, pair->second);
  }
  core->r[2] = to.index;
}

} // namespace

void registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kFetchFrames, "Crash 2 pose blend", fetchOverride);
}

} // namespace crash2::pose_blend
