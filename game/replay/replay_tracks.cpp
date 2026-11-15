#include "replay/replay_tracks.h"

#include "blend.h"

namespace crash {
namespace {

using psx::present::Motion;
using psx::present::MotionField;

constexpr std::size_t kFirstScale = 6;
constexpr psx::present::RecordByte kObjectLive{.offset = 0x00u, .low = 1, .high = 1};

std::array<MotionField, 6> cameraFields() {
  return {{{.offset = 0x00u, .motion = Motion::Linear},
           {.offset = 0x04u, .motion = Motion::Linear},
           {.offset = 0x08u, .motion = Motion::Linear},
           {.offset = 0x0Cu, .motion = Motion::Angle12Low},
           {.offset = 0x10u, .motion = Motion::Angle12Low},
           {.offset = 0x14u, .motion = Motion::Angle12Low}}};
}

std::array<MotionField, 11> objectFields(const ObjectLayout &object) {
  return {{{.offset = object.trans, .motion = Motion::Linear},
           {.offset = object.trans + 4u, .motion = Motion::Linear},
           {.offset = object.trans + 8u, .motion = Motion::Linear},
           {.offset = object.rot, .motion = Motion::Angle12Low},
           {.offset = object.rot + 4u, .motion = Motion::Angle12Low},
           {.offset = object.rot + 8u, .motion = Motion::Angle12Low},
           {.offset = object.scale, .motion = Motion::Linear},
           {.offset = object.scale + 4u, .motion = Motion::Linear},
           {.offset = object.scale + 8u, .motion = Motion::Linear},
           // The pose between two ticks is each title's pose owner's.
           {.offset = object.anim, .motion = Motion::Step},
           {.offset = object.animFrame, .motion = Motion::Step}}};
}

} // namespace

void ReplayTracks::Placement::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  for (std::size_t i = 0; i < fields_.size(); ++i) {
    const std::uint32_t from = samples.from[i];
    const std::uint32_t to = samples.to[i];
    const bool flips = i >= kFirstScale && i < kFirstScale + 3 && ((from ^ to) & 0x80000000u) != 0;
    out[i] = flips ? to : psx::present::blendedWord(fields_[i].motion, from, to, t);
  }
}

ReplayTracks::ReplayTracks(const TrackFacts &facts)
    : camera_(cameraFields()), object_(objectFields(facts.object)), keys_{{{.offset = facts.object.exec}}},
      placement_(object_),
      tracks_{{
          {.name = "camera", .address = facts.camera, .fields = camera_},
          {.name = "objects",
           .address = facts.objectPool,
           .fields = object_,
           .rule = &placement_,
           .keys = keys_,
           .records =
               {.throughPointer = true, .stride = facts.objectStride, .count = facts.objectCount, .live = kObjectLive}},
          {.name = "player",
           .address = facts.player,
           .fields = object_,
           .rule = &placement_,
           .keys = keys_,
           .records = {.throughPointer = true, .stride = facts.objectStride, .count = 1, .live = kObjectLive}},
      }} {}

} // namespace crash
