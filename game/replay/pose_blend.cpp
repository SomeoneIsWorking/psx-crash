#include "replay/pose_blend.h"

#include "core.h"
#include "native_dispatch.h"
#include "runtime/session.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace crash {
namespace {

struct Frames {
  std::uint32_t first = 0;
  std::uint32_t second = 0;
  bool operator==(const Frames &) const = default;
};

struct Pose {
  std::uint32_t anim = 0;
  std::uint32_t frame = 0;
};

void callFetch(Core &core, const PoseFacts &facts, std::uint32_t object) {
  core.r[4] = object;
  core.r[5] = object + facts.scale;
  psx::cpu::callOriginalToReturn(
      core, facts.fetchPose, psx::cpu::ExecutionBudget::currentTurn(core), facts.reader.owner);
}

// The frames the pose fetch puts in the object's block for `pose`.
Frames fetch(Core &core, const PoseFacts &facts, std::uint32_t object, Pose pose) {
  core.mem_w32(object + facts.anim, pose.anim);
  core.mem_w32(object + facts.animFrame, pose.frame);
  callFetch(core, facts, object);
  const std::uint32_t block = core.mem_r32(object + facts.block);
  return {core.mem_r32(block + facts.firstFrame), core.mem_r32(block + facts.secondFrame)};
}

// The pair the queue draws and its mix.
struct Morph {
  Frames pair;
  MorphReader::Mix mix;
};

// The mix whose sum is the four frames' mean. Two frames (or one) are one pair: x x y y draws (x, y) at 2 and 2
// quarters, x x x y at 3 and 1, x y y y at 1 and 3; a tick's own pair keeps its order. Three or four frames draw the
// pair that differs and the other beside it, a quarter on each frame.
Morph quarterPose(const Frames &from, const Frames &to) {
  const Frames pair = to.first != to.second ? to : from.first != from.second ? from : Frames{from.first, to.first};
  const std::array<std::uint32_t, 4> frames{from.first, from.second, to.first, to.second};
  if (std::ranges::any_of(frames, [&](std::uint32_t f) {
        return f != pair.first && f != pair.second;
      })) {
    // The queue sends a pair of one frame to the static drawer, so the packet carries a pair that differs.
    const bool later = to.first != to.second;
    const Frames other = later ? from : to;
    return {.pair = later ? to : from, .mix = {.quarters = {1, 1, 1, 1}, .second = {other.first, other.second}}};
  }
  if (pair.first == pair.second) {
    return {.pair = pair};
  }
  const auto k = static_cast<std::uint32_t>(std::ranges::count(frames, pair.second));
  return {.pair = pair, .mix = {.quarters = {4u - k, k, 0, 0}}};
}

} // namespace

// The pose of a vertex object the replay queues halfway between its ticks, written to its block before the queue reads
// it; the mix its morph draw takes.
MorphReader::Mix PoseBlend::blendPose(Core &core, std::uint32_t object) const {
  DrawReplay &replay = session(core).drawReplay();
  const std::optional<float> t = replay.frame().replaying();
  const std::uint32_t anim = core.mem_r32(object + facts_.anim);
  const std::uint32_t block = core.mem_r32(object + facts_.block);
  if (!t || *t != 0.5f || anim == 0u || block == 0u || core.mem_r8(anim) != facts_.vertexKind) {
    return {};
  }
  const std::optional<psx::present::MotionRule::Samples> samples = replay.pairedObject(object);
  if (!samples) {
    return {};
  }
  const Pose later{.anim = anim, .frame = core.mem_r32(object + facts_.animFrame)};
  const Frames drawn{core.mem_r32(block + facts_.firstFrame), core.mem_r32(block + facts_.secondFrame)};
  const Frames from =
      fetch(core,
            facts_,
            object,
            {.anim = samples->from[ReplayTracks::kAnimField], .frame = samples->from[ReplayTracks::kAnimFrameField]});
  const Frames to = fetch(core, facts_, object, later);
  const std::uint32_t model = core.mem_r32(drawn.first + facts_.frameModel);
  const std::array<std::uint32_t, 4> frames{from.first, from.second, to.first, to.second};
  // A block that does not hold its animation's frames, or frames of another model, is drawn as it stands.
  const bool blends = to == drawn && std::ranges::all_of(frames, [&](std::uint32_t f) {
                        return core.mem_r32(f + facts_.frameModel) == model;
                      });
  const Morph shown = blends ? quarterPose(from, to) : Morph{.pair = drawn};
  core.mem_w32(block + facts_.firstFrame, shown.pair.first);
  core.mem_w32(block + facts_.secondFrame, shown.pair.second);
  return shown.mix;
}

void PoseBlend::beforeQueue(Core &core, std::uint32_t object) {
  mix_ = blendPose(core, object);
}

void PoseBlend::queued(Core &, std::uint32_t packet) {
  reader_.weigh(packet, mix_);
}

void PoseBlend::fetchPose(Core &core, std::uint32_t object) {
  const std::uint32_t anim = core.mem_r32(object + facts_.anim);
  if (anim != 0u && core.mem_r8(anim) == facts_.vertexKind) {
    callFetch(core, facts_, object);
  }
}

} // namespace crash
