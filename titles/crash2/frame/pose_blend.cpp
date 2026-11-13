#include "crash2/frame/pose_blend.h"

#include "core.h"
#include "crash2/entry/session.h"
#include "crash2/render/morph_reader.h"
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
// g_BuildBuffer and its GfxFrame's packet list head, which Gfx_QueueObject pushes the object's packet onto.
constexpr std::uint32_t kBuildBuffer = 0x80063458u;
constexpr std::uint32_t kPacketHead = 0x2088u;

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

// The pose of a vertex object the replay queues halfway between its ticks, written to its block before the queue reads
// it; the mix its morph draw takes.
MorphReader::Mix blendPose(Core &core, std::uint32_t object) {
  DrawReplay &replay = session(core).drawReplay;
  const std::optional<float> t = replay.frame().replaying();
  const std::uint32_t anim = core.mem_r32(object + kAnim);
  const std::uint32_t block = core.mem_r32(object + kBlock);
  if (!t || *t != 0.5f || anim == 0u || block == 0u || core.mem_r8(anim) != kVertexKind) {
    return {};
  }
  const std::optional<psx::present::MotionRule::Samples> samples = replay.pairedObject(object);
  if (!samples) {
    return {};
  }
  const Pose later{.anim = anim, .frame = core.mem_r32(object + kAnimFrame)};
  const Frames drawn{core.mem_r32(block + kFirstFrame), core.mem_r32(block + kSecondFrame)};
  const Frames from =
      fetch(core,
            object,
            {.anim = samples->from[DrawReplay::kAnimField], .frame = samples->from[DrawReplay::kAnimFrameField]});
  const Frames to = fetch(core, object, later);
  const std::uint32_t model = core.mem_r32(drawn.first + kFrameModel);
  const std::array<std::uint32_t, 4> frames{from.first, from.second, to.first, to.second};
  // A block that does not hold its animation's frames, or frames of another model, is drawn as it stands.
  const bool blends = to == drawn && std::ranges::all_of(frames, [&](std::uint32_t f) {
                        return core.mem_r32(f + kFrameModel) == model;
                      });
  const Morph shown = blends ? quarterPose(from, to) : Morph{.pair = drawn};
  core.mem_w32(block + kFirstFrame, shown.pair.first);
  core.mem_w32(block + kSecondFrame, shown.pair.second);
  return shown.mix;
}

void queueOverride(Core *core) {
  const std::uint32_t object = core->r[4];
  const std::uint32_t place = core->r[5];
  session(*core).drawReplay.walk().queued(*core, object);
  const MorphReader::Mix mix = blendPose(*core, object);
  const std::uint32_t frame = core->mem_r32(kBuildBuffer);
  const std::uint32_t head = core->mem_r32(frame + kPacketHead);
  core->r[4] = object;
  core->r[5] = place;
  psx::cpu::callOriginalToReturn(
      *core, kQueueObject, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 2 queue object");
  if (const std::uint32_t packet = core->mem_r32(frame + kPacketHead); packet != head) {
    session(*core).morphReader.weigh(packet, mix);
  }
}

} // namespace

void registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kQueueObject, "Crash 2 pose blend", queueOverride);
}

} // namespace crash2::pose_blend
