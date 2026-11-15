#include "crash1/frame/pose_blend.h"

#include "blend.h"
#include "core.h"
#include "crash1/entry/session.h"
#include "crash1/frame/frame_facts.h"
#include "native_dispatch.h"

#include <algorithm>
#include <lucent/log.h>

namespace crash1 {
namespace {

namespace svtx = frame_facts::svtx;
using psx::present::lerpInt;

std::uint32_t frameBytes(Core &core, std::uint32_t frame) {
  return svtx::kVertices + core.mem_r32(frame + svtx::kVertexCount) * svtx::kVertexBytes;
}

std::uint32_t wordAt(const std::vector<std::uint8_t> &bytes, std::uint32_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) | static_cast<std::uint32_t>(bytes[offset + 1u]) << 8 |
         static_cast<std::uint32_t>(bytes[offset + 2u]) << 16 | static_cast<std::uint32_t>(bytes[offset + 3u]) << 24;
}

std::vector<std::uint8_t> readFrame(Core &core, std::uint32_t frame, std::uint32_t size) {
  std::vector<std::uint8_t> bytes(size);
  for (std::uint32_t i = 0; i < size; ++i) {
    bytes[i] = core.mem_r8(frame + i);
  }
  return bytes;
}

void writeFrame(Core &core, std::uint32_t frame, const std::vector<std::uint8_t> &bytes) {
  for (std::uint32_t i = 0; i < bytes.size(); ++i) {
    core.mem_w8(frame + i, bytes[i]);
  }
}

// The origin, box and collision point move linearly; a vertex's position bytes are unsigned, its normal bytes signed
// and its colour bytes unsigned.
std::vector<std::uint8_t>
blendFrames(const std::vector<std::uint8_t> &from, const std::vector<std::uint8_t> &to, float t, bool signedTail) {
  std::vector<std::uint8_t> out = to;
  for (std::uint32_t offset = svtx::kOrigin; offset < svtx::kVertices; offset += 4u) {
    const auto word = static_cast<std::uint32_t>(
        lerpInt(static_cast<std::int32_t>(wordAt(from, offset)), static_cast<std::int32_t>(wordAt(to, offset)), t));
    for (std::uint32_t i = 0; i < 4u; ++i) {
      out[offset + i] = static_cast<std::uint8_t>(word >> (8u * i));
    }
  }
  for (std::uint32_t offset = svtx::kVertices; offset < out.size(); ++offset) {
    const bool normal = signedTail && (offset - svtx::kVertices) % svtx::kVertexBytes >= svtx::kPositionBytes;
    const std::int32_t a = normal ? static_cast<std::int8_t>(from[offset]) : from[offset];
    const std::int32_t b = normal ? static_cast<std::int8_t>(to[offset]) : to[offset];
    out[offset] = static_cast<std::uint8_t>(lerpInt(a, b, t));
  }
  return out;
}

} // namespace

void PoseBlend::openPass() {
  recording_ = true;
  drawn_.swap(drawing_);
  drawing_.clear();
}

void PoseBlend::closePass() {
  recording_ = false;
}

void PoseBlend::report() const {
  lucent::info("crash1-replay",
               "poses: SVTX {} drawn blended, {} drawn as the later tick (no matching frame last tick); CVTX {}, {}",
               blended_[0],
               unpaired_[0],
               blended_[1],
               unpaired_[1]);
}

const PoseBlend::Frame *PoseBlend::drawnFrame(std::uint32_t object) const {
  const auto it = std::ranges::find(drawn_, object, &Frame::object);
  return it == drawn_.end() ? nullptr : &*it;
}

void PoseBlend::transform(Core &core, Kind kind, std::uint32_t frame, std::uint32_t object, std::optional<float> t) {
  const auto original = [&core, kind] {
    psx::cpu::callOriginalToReturn(core,
                                   kind == Kind::Svtx ? frame_facts::kTransformSvtx : frame_facts::kTransformCvtx,
                                   psx::cpu::ExecutionBudget::currentTurn(core),
                                   kind == Kind::Svtx ? "Crash 1 SVTX frame" : "Crash 1 CVTX frame");
  };
  const std::uint32_t size = frameBytes(core, frame);
  const std::uint32_t exec = core.mem_r32(object + frame_facts::object::kExec);
  if (!t) {
    if (recording_ && std::ranges::find(drawing_, object, &Frame::object) == drawing_.end()) {
      drawing_.push_back({.object = object, .exec = exec, .kind = kind, .bytes = readFrame(core, frame, size)});
    }
    original();
    return;
  }
  const Frame *earlier = drawnFrame(object);
  std::vector<std::uint8_t> later = readFrame(core, frame, size);
  if (earlier == nullptr || earlier->exec != exec || earlier->kind != kind || earlier->bytes.size() != later.size() ||
      wordAt(earlier->bytes, svtx::kModel) != wordAt(later, svtx::kModel)) {
    ++unpaired_[static_cast<std::size_t>(kind)];
    original();
    return;
  }
  ++blended_[static_cast<std::size_t>(kind)];
  writeFrame(core, frame, blendFrames(earlier->bytes, later, *t, kind == Kind::Svtx));
  original();
  // Another object may draw the same frame later in this replay.
  writeFrame(core, frame, later);
}

void PoseBlend::svtxOverride(Core *core) {
  Session &owner = session(*core);
  owner.drawReplay.poses().transform(*core, Kind::Svtx, core->r[4], core->r[6], owner.drawReplay.frame().replaying());
}

void PoseBlend::cvtxOverride(Core *core) {
  Session &owner = session(*core);
  owner.drawReplay.poses().transform(*core, Kind::Cvtx, core->r[4], core->r[6], owner.drawReplay.frame().replaying());
}

void PoseBlend::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, frame_facts::kTransformSvtx, "Crash 1 SVTX pose blend", svtxOverride);
  psx::cpu::installNativeOverride(core, frame_facts::kTransformCvtx, "Crash 1 CVTX pose blend", cvtxOverride);
}

} // namespace crash1
