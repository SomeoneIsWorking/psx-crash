#include "crash2/frame/draw_replay.h"

#include "blend.h"
#include "core.h"
#include "crash2/entry/session.h"
#include "native_dispatch.h"

#include <array>
#include <optional>
#include <span>

namespace crash2 {
namespace {

using psx::present::Motion;
using psx::present::MotionField;
using psx::present::MotionKey;
using psx::present::MotionTrack;
using psx::present::RecordByte;

// Names and layouts from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr std::uint32_t kSampleStub = 0x80011E88u;
constexpr std::uint32_t kGfxUpdateView = 0x80017BC4u;
constexpr std::uint32_t kCameraX = 0x800607F4u;
constexpr std::uint32_t kObjPool = 0x8006CB14u;
constexpr std::uint32_t kObjStride = 0x23Cu;
constexpr std::uint32_t kObjCount = 0x60u;
// The player's record, allocated apart from the pool.
constexpr std::uint32_t kPlayer = 0x8005F38Cu;

// g_CameraX/Y/Z (24.8), then g_CameraAngleX/Y/Z (4096 a turn).
constexpr std::array<MotionField, 6> kCameraFields{{
    {.offset = 0x00u, .motion = Motion::Linear},
    {.offset = 0x04u, .motion = Motion::Linear},
    {.offset = 0x08u, .motion = Motion::Linear},
    {.offset = 0x0Cu, .motion = Motion::Angle12Low},
    {.offset = 0x10u, .motion = Motion::Angle12Low},
    {.offset = 0x14u, .motion = Motion::Angle12Low},
}};

// GoolObject pos (+0x60), rot (+0x6C, 4096 a turn), scale (+0x78, x negated to flip) and animation.
constexpr std::array<MotionField, 11> kObjectFields{{
    {.offset = 0x60u, .motion = Motion::Linear},
    {.offset = 0x64u, .motion = Motion::Linear},
    {.offset = 0x68u, .motion = Motion::Linear},
    {.offset = 0x6Cu, .motion = Motion::Angle12Low},
    {.offset = 0x70u, .motion = Motion::Angle12Low},
    {.offset = 0x74u, .motion = Motion::Angle12Low},
    {.offset = 0x78u, .motion = Motion::Linear},
    {.offset = 0x7Cu, .motion = Motion::Linear},
    {.offset = 0x80u, .motion = Motion::Linear},
    // The animation and its frame << 8; the pose between two ticks is pose_blend's.
    {.offset = 0xE8u, .motion = Motion::Step},
    {.offset = 0xECu, .motion = Motion::Step},
}};
static_assert(kObjectFields[DrawReplay::kAnimField].offset == 0xE8u);
static_assert(kObjectFields[DrawReplay::kAnimFrameField].offset == 0xECu);
constexpr std::size_t kFirstScale = 6;
// The object's executable: a slot freed and reused by another kind does not pair.
constexpr std::array<MotionKey, 1> kObjectKeys{{{.offset = 0x0Cu}}};

// Position and angles by their fields' motion; a scale whose sign changes (a flip) steps.
class ObjectPose final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override {
    for (std::size_t i = 0; i < kObjectFields.size(); ++i) {
      const std::uint32_t from = samples.from[i];
      const std::uint32_t to = samples.to[i];
      const bool flips = i >= kFirstScale && i < kFirstScale + 3 && ((from ^ to) & 0x80000000u) != 0;
      out[i] = flips ? to : psx::present::blendedWord(kObjectFields[i].motion, from, to, t);
    }
  }
};
const ObjectPose kObjectPose;

// Obj_Init sets an object's first word to 1, Obj_Kill to 0.
constexpr RecordByte kObjectLive{.offset = 0x00u, .low = 1, .high = 1};

const std::array<MotionTrack, 3> kTracks{{
    {.name = "camera", .address = kCameraX, .fields = kCameraFields},
    {.name = "objects",
     .address = kObjPool,
     .fields = kObjectFields,
     .rule = &kObjectPose,
     .keys = kObjectKeys,
     .records = {.throughPointer = true, .stride = kObjStride, .count = kObjCount, .live = kObjectLive}},
    {.name = "player",
     .address = kPlayer,
     .fields = kObjectFields,
     .rule = &kObjectPose,
     .keys = kObjectKeys,
     .records = {.throughPointer = true, .stride = kObjStride, .count = 1, .live = kObjectLive}},
}};

void sampleOverride(Core *core) {
  session(*core).drawReplay.frame().beforeDraw(*core);
}

void updateViewOverride(Core *core) {
  DrawReplay &replay = session(*core).drawReplay;
  if (!replay.frame().replaying()) {
    replay.view().record(*core);
  }
  psx::cpu::callOriginalToReturn(
      *core, kGfxUpdateView, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 2 view entry");
}

} // namespace

DrawReplay::DrawReplay() : frame_(section_, kTracks, sources_) {}

std::optional<psx::present::MotionRule::Samples> DrawReplay::pairedObject(std::uint32_t object) const {
  for (const MotionTrack &track : std::span(kTracks).subspan(1)) {
    if (auto samples = frame_.motion().pairedRecord(track, object)) {
      return samples;
    }
  }
  return std::nullopt;
}

void DrawReplay::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kSampleStub, "Crash 2 draw replay sample", sampleOverride);
  psx::cpu::installNativeOverride(core, kGfxUpdateView, "Crash 2 view entry", updateViewOverride);
  FadeLevel::registerOverride(core);
  ObjectWalk::registerOverrides(core);
}

} // namespace crash2
