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

// A pair of frames with k quarters on the second.
struct Morph {
  Frames pair;
  std::uint32_t quarters = MorphReader::kRetailQuarters;
};

// The pair and weight whose sum is the four frames' mean, when they are two frames (or one): x x y y draws (x, y) at
// k = 2, x x x y at k = 1, x y y y at k = 3. A tick's own pair keeps its order.
std::optional<Morph> quarterPose(const Frames &from, const Frames &to) {
  const Frames pair = to.first != to.second ? to : from.first != from.second ? from : Frames{from.first, to.first};
  const std::array<std::uint32_t, 4> frames{from.first, from.second, to.first, to.second};
  if (std::ranges::any_of(frames, [&](std::uint32_t f) {
        return f != pair.first && f != pair.second;
      })) {
    return std::nullopt;
  }
  if (pair.first == pair.second) {
    return Morph{.pair = pair};
  }
  return Morph{.pair = pair, .quarters = static_cast<std::uint32_t>(std::ranges::count(frames, pair.second))};
}

// The pose of a vertex object the replay queues halfway between its ticks, written to its block before the queue reads
// it; the weight its morph draw takes.
std::uint32_t blendPose(Core &core, std::uint32_t object) {
  DrawReplay &replay = session(core).drawReplay;
  const std::optional<float> t = replay.frame().replaying();
  const std::uint32_t anim = core.mem_r32(object + kAnim);
  const std::uint32_t block = core.mem_r32(object + kBlock);
  if (!t || *t != 0.5f || anim == 0u || block == 0u || core.mem_r8(anim) != kVertexKind) {
    return MorphReader::kRetailQuarters;
  }
  const std::optional<psx::present::MotionRule::Samples> samples = replay.pairedObject(object);
  if (!samples) {
    return MorphReader::kRetailQuarters;
  }
  const Pose later{.anim = anim, .frame = core.mem_r32(object + kAnimFrame)};
  const Frames drawn{core.mem_r32(block + kFirstFrame), core.mem_r32(block + kSecondFrame)};
  const Frames from =
      fetch(core,
            object,
            {.anim = samples->from[DrawReplay::kAnimField], .frame = samples->from[DrawReplay::kAnimFrameField]});
  const Frames to = fetch(core, object, later);
  // A block that does not hold its animation's frames is drawn as it stands.
  const std::optional<Morph> morph = to == drawn ? quarterPose(from, to) : std::nullopt;
  const Morph shown =
      morph && core.mem_r32(morph->pair.first + kFrameModel) == core.mem_r32(morph->pair.second + kFrameModel)
          ? *morph
          : Morph{.pair = drawn};
  core.mem_w32(block + kFirstFrame, shown.pair.first);
  core.mem_w32(block + kSecondFrame, shown.pair.second);
  return shown.quarters;
}

void queueOverride(Core *core) {
  const std::uint32_t object = core->r[4];
  const std::uint32_t place = core->r[5];
  const std::uint32_t quarters = blendPose(*core, object);
  const std::uint32_t frame = core->mem_r32(kBuildBuffer);
  const std::uint32_t head = core->mem_r32(frame + kPacketHead);
  core->r[4] = object;
  core->r[5] = place;
  psx::cpu::callOriginalToReturn(
      *core, kQueueObject, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 2 queue object");
  if (const std::uint32_t packet = core->mem_r32(frame + kPacketHead); packet != head) {
    session(*core).morphReader.weigh(packet, quarters);
  }
}

} // namespace

void registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kQueueObject, "Crash 2 pose blend", queueOverride);
}

} // namespace crash2::pose_blend
