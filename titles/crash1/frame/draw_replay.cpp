#include "crash1/frame/draw_replay.h"

#include "blend.h"
#include "core.h"
#include "crash1/entry/session.h"
#include "crash1/frame/frame_facts.h"
#include "native_dispatch.h"

#include <array>
#include <span>

namespace crash1 {
namespace {

using psx::present::Motion;
using psx::present::MotionField;
using psx::present::MotionKey;
using psx::present::MotionTrack;
using psx::present::RecordByte;
namespace facts = frame_facts;
namespace object = frame_facts::object;

constexpr std::array<MotionField, 6> kCameraFields{{
    {.offset = 0x00u, .motion = Motion::Linear},
    {.offset = 0x04u, .motion = Motion::Linear},
    {.offset = 0x08u, .motion = Motion::Linear},
    {.offset = 0x0Cu, .motion = Motion::Angle12Low},
    {.offset = 0x10u, .motion = Motion::Angle12Low},
    {.offset = 0x14u, .motion = Motion::Angle12Low},
}};

constexpr std::array<MotionField, 11> kObjectFields{{
    {.offset = object::kTrans, .motion = Motion::Linear},
    {.offset = object::kTrans + 4u, .motion = Motion::Linear},
    {.offset = object::kTrans + 8u, .motion = Motion::Linear},
    {.offset = object::kRot, .motion = Motion::Angle12Low},
    {.offset = object::kRot + 4u, .motion = Motion::Angle12Low},
    {.offset = object::kRot + 8u, .motion = Motion::Angle12Low},
    {.offset = object::kScale, .motion = Motion::Linear},
    {.offset = object::kScale + 4u, .motion = Motion::Linear},
    {.offset = object::kScale + 8u, .motion = Motion::Linear},
    {.offset = object::kAnimSeq, .motion = Motion::Step},
    {.offset = object::kAnimFrame, .motion = Motion::Step},
}};
constexpr std::size_t kFirstScale = 6;
// The object's executable: a slot freed and reused by another kind does not pair.
constexpr std::array<MotionKey, 1> kObjectKeys{{{.offset = object::kExec}}};

// Position and angles by their fields' motion; a scale whose sign changes (a flip) steps.
class ObjectPlacement final : public psx::present::MotionRule {
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
const ObjectPlacement kObjectPlacement;

constexpr RecordByte kObjectLive{.offset = 0x00u, .low = 1, .high = 1};

const std::array<MotionTrack, 3> kTracks{{
    {.name = "camera", .address = facts::kCamera, .fields = kCameraFields},
    {.name = "objects",
     .address = facts::kObjectPool,
     .fields = kObjectFields,
     .rule = &kObjectPlacement,
     .keys = kObjectKeys,
     .records =
         {.throughPointer = true, .stride = facts::kObjectStride, .count = facts::kObjectCount, .live = kObjectLive}},
    {.name = "player",
     .address = facts::kPlayer,
     .fields = kObjectFields,
     .rule = &kObjectPlacement,
     .keys = kObjectKeys,
     .records = {.throughPointer = true, .stride = facts::kObjectStride, .count = 1, .live = kObjectLive}},
}};

void updateObjectsOverride(Core *core) {
  const std::uint32_t returnAddress = core->r[31];
  DrawReplay &replay = session(*core).drawReplay;
  const bool real = returnAddress == facts::kObjectsReturn && !replay.frame().replaying();
  if (real) {
    replay.poses().openPass();
    replay.screen().openPass();
  }
  psx::cpu::callOriginalToReturn(
      *core, facts::kGoolUpdateObjects, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 1 object update");
  if (real) {
    replay.poses().closePass();
    replay.frame().beforeDraw(*core);
  }
}

// The frame's input was read by the real walk; a replay's walk reads none.
void padUpdateOverride(Core *core) {
  if (session(*core).drawReplay.frame().replaying()) {
    return;
  }
  psx::cpu::callOriginalToReturn(
      *core, facts::kPadUpdate, psx::cpu::ExecutionBudget::currentTurn(*core), "Crash 1 pad update");
}

} // namespace

DrawReplay::DrawReplay() : frame_(section_, kTracks) {}

void DrawReplay::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, facts::kGoolUpdateObjects, "Crash 1 draw replay sample", updateObjectsOverride);
  psx::cpu::installNativeOverride(core, facts::kPadUpdate, "Crash 1 pad update", padUpdateOverride);
  PoseBlend::registerOverride(core);
  ScreenEffects::registerOverrides(core);
}

} // namespace crash1
