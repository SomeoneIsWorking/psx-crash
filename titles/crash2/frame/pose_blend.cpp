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
constexpr std::uint32_t kQueueObject = 0x8001ABF8u;
constexpr std::uint32_t kFetchPose = 0x8001A4A4u;
constexpr std::uint32_t kBlock = 0x08u;
constexpr std::uint32_t kScale = 0x78u;
constexpr std::uint32_t kAnim = 0xE8u;
constexpr std::uint32_t kAnimFrame = 0xECu;
// The block's two vertex frames, which Gfx_QueueObject's kind 1 draws.
constexpr std::uint32_t kFirstFrame = 0x20u;
constexpr std::uint32_t kSecondFrame = 0x24u;
constexpr std::uint8_t kVertexKind = 1;
// A vertex frame's model entry id, which both frames of a morph must share.
constexpr std::uint32_t kFrameModel = 0x10u;

struct Frames {
  std::uint32_t first = 0;
  std::uint32_t second = 0;
  bool operator==(const Frames &) const = default;
};

struct Pose {
  std::uint32_t anim = 0;
  std::uint32_t frame = 0;
};

// The frames func_8001A4A4 puts in the object's block for `pose`.
Frames fetch(Core &core, std::uint32_t object, Pose pose) {
  core.mem_w32(object + kAnim, pose.anim);
  core.mem_w32(object + kAnimFrame, pose.frame);
  core.r[4] = object;
  core.r[5] = object + kScale;
  psx::cpu::callOriginalToReturn(core, kFetchPose, psx::cpu::ExecutionBudget::currentTurn(core), "Crash 2 pose");
  const std::uint32_t block = core.mem_r32(object + kBlock);
  return {core.mem_r32(block + kFirstFrame), core.mem_r32(block + kSecondFrame)};
}

// The pair whose equal-weight sum is the four frames' mean, when there is one.
std::optional<Frames> halfway(const Frames &from, const Frames &to) {
  std::array<std::uint32_t, 4> frames{from.first, from.second, to.first, to.second};
  std::ranges::sort(frames);
  if (frames[0] != frames[1] || frames[2] != frames[3]) {
    return std::nullopt;
  }
  return Frames{frames[0], frames[2]};
}

// The halfway pose of a vertex object the replay queues, written to its block before the queue reads it.
void blendPose(Core &core, std::uint32_t object) {
  DrawReplay &replay = session(core).drawReplay;
  const std::optional<float> t = replay.frame().replaying();
  const std::uint32_t anim = core.mem_r32(object + kAnim);
  const std::uint32_t block = core.mem_r32(object + kBlock);
  if (!t || *t != 0.5f || anim == 0u || block == 0u || core.mem_r8(anim) != kVertexKind) {
    return;
  }
  const std::optional<psx::present::MotionRule::Samples> samples = replay.pairedObject(object);
  if (!samples) {
    return;
  }
  const Pose later{.anim = anim, .frame = core.mem_r32(object + kAnimFrame)};
  const Frames drawn{core.mem_r32(block + kFirstFrame), core.mem_r32(block + kSecondFrame)};
  const Frames from =
      fetch(core,
            object,
            {.anim = samples->from[DrawReplay::kAnimField], .frame = samples->from[DrawReplay::kAnimFrameField]});
  const Frames to = fetch(core, object, later);
  // A block that does not hold its animation's frames is drawn as it stands.
  const std::optional<Frames> pair = to == drawn ? halfway(from, to) : std::nullopt;
  const Frames shown =
      pair && core.mem_r32(pair->first + kFrameModel) == core.mem_r32(pair->second + kFrameModel) ? *pair : drawn;
  core.mem_w32(block + kFirstFrame, shown.first);
  core.mem_w32(block + kSecondFrame, shown.second);
}

void queueOverride(Core *core) {
  const std::uint32_t object = core->r[4];
  const std::uint32_t place = core->r[5];
  blendPose(*core, object);
  core->r[4] = object;
  core->r[5] = place;
  psx::cpu::callOriginalToReturn(
      *core, kQueueObject, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 2 queue object");
}

} // namespace

void registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kQueueObject, "Crash 2 pose blend", queueOverride);
}

} // namespace crash2::pose_blend
