#include "crash2/frame/draw_replay.h"

#include "core.h"
#include "crash2/entry/session.h"
#include "native_dispatch.h"

#include <array>

namespace crash2 {
namespace {

using psx::present::Motion;
using psx::present::MotionField;
using psx::present::MotionKey;
using psx::present::MotionTrack;
using psx::present::RecordByte;

// Names and layouts from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr std::uint32_t kSampleStub = 0x80011E88u;
constexpr std::uint32_t kCameraX = 0x800607F4u;
constexpr std::uint32_t kObjPool = 0x8006CB14u;
constexpr std::uint32_t kObjStride = 0x23Cu;
constexpr std::uint32_t kObjCount = 0x60u;

// g_CameraX/Y/Z (24.8), then g_CameraAngleX/Y/Z (4096 a turn).
constexpr std::array<MotionField, 6> kCameraFields{{
    {.offset = 0x00u, .motion = Motion::Linear},
    {.offset = 0x04u, .motion = Motion::Linear},
    {.offset = 0x08u, .motion = Motion::Linear},
    {.offset = 0x0Cu, .motion = Motion::Angle12Low},
    {.offset = 0x10u, .motion = Motion::Angle12Low},
    {.offset = 0x14u, .motion = Motion::Angle12Low},
}};

// Object position (+0x60) and rotation (+0x6C, 4096 a turn); scale and animation frames step.
constexpr std::array<MotionField, 6> kObjectFields{{
    {.offset = 0x60u, .motion = Motion::Linear},
    {.offset = 0x64u, .motion = Motion::Linear},
    {.offset = 0x68u, .motion = Motion::Linear},
    {.offset = 0x6Cu, .motion = Motion::Angle12Low},
    {.offset = 0x70u, .motion = Motion::Angle12Low},
    {.offset = 0x74u, .motion = Motion::Angle12Low},
}};
// The object's executable: a slot freed and reused by another kind does not pair.
constexpr std::array<MotionKey, 1> kObjectKeys{{{.offset = 0x0Cu}}};

const std::array<MotionTrack, 2> kTracks{{
    {.name = "camera", .address = kCameraX, .fields = kCameraFields},
    {.name = "objects",
     .address = kObjPool,
     .fields = kObjectFields,
     .keys = kObjectKeys,
     // Obj_Init sets the first word to 1, Obj_Kill to 0.
     .records = {.throughPointer = true,
                 .stride = kObjStride,
                 .count = kObjCount,
                 .live = RecordByte{.offset = 0x00u, .low = 1, .high = 1}}},
}};

void sampleOverride(Core *core) {
  session(*core).drawReplay.frame().beforeDraw(*core);
}

} // namespace

DrawReplay::DrawReplay() : frame_(section_, kTracks) {}

void DrawReplay::registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kSampleStub, "Crash 2 draw replay sample", sampleOverride);
}

} // namespace crash2
