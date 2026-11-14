#include "crash2/render/world_renderer.h"

#include "core.h"
#include "crash2/render/world_steps.h"
#include "execution_services.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "r3000.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace crash2 {
namespace {

using namespace psx::cpu;
using world::Pass;
using world::run;
namespace scratch = world::scratch;
namespace block = world::block;

inline constexpr std::uint32_t kDrawWorlds = 0x80041E5Cu;
inline constexpr std::uint32_t kResetOt = 0x8003BC5Cu;
inline constexpr std::uint32_t kResetOtReturn = 0x80041ED8u;
inline constexpr std::uint32_t kSetupReturn = 0x80041F3Cu;
inline constexpr std::uint32_t kDepthShiftTable = 0x8005C724u;
inline constexpr std::uint32_t kDepthShiftWords = 8u;
inline constexpr std::uint32_t kFrameSize = 0x44u;
inline constexpr std::uint32_t kFrameOt = 0x18u;
inline constexpr std::uint32_t kFrameNext = 0x08u;
inline constexpr std::uint32_t kOtSlots = 0x800u;
inline constexpr std::uint32_t kOtTop = 0x1FFCu;
inline constexpr std::uint32_t kOtEnd = 0x00FFFFFFu;
inline constexpr std::uint32_t kListFirst = 4u;
inline constexpr std::uint32_t kSceneModes = 0x10u;
inline constexpr std::uint32_t kVertexZMask = 0xFFF0u;
inline constexpr std::uint32_t kCornerMask = 0xFFFu;
inline constexpr std::uint32_t kMaterialShift = 4u;
inline constexpr std::uint32_t kMaterialLow = 0xFu;
inline constexpr std::uint32_t kCameraShift = 8u;
inline constexpr std::string_view kOwner = "Crash 2 Gfx_DrawWorlds";

constexpr std::array<world::Mode, 10> kModes{{
    {0x8003DD0Cu,
     0x80042420u,
     0x800426A0u,
     0x80042548u,
     0x80042818u,
     world::projectTriangle,
     world::projectQuad,
     world::drawTriangle,
     world::drawQuad},
    {0x8004599Cu,
     0x80042420u,
     0x800426A0u,
     0x800459A8u,
     0x80045AB4u,
     world::projectTriangle,
     world::projectQuad,
     world::drawFogTriangle,
     world::drawFogQuad},
    {0x8004399Cu,
     0x80042420u,
     0x800426A0u,
     0x800439D4u,
     0x80043A84u,
     world::projectTriangle,
     world::projectQuad,
     world::drawDepthFogTriangle,
     world::drawDepthFogQuad},
    {0x80044980u,
     0x80042420u,
     0x800426A0u,
     0x800449B0u,
     0x80044A04u,
     world::projectTriangle,
     world::projectQuad,
     world::drawTintTriangle,
     world::drawTintQuad},
    {0x80043D54u,
     0x80042420u,
     0x800426A0u,
     0x80043D84u,
     0x80043EA0u,
     world::projectTriangle,
     world::projectQuad,
     world::drawPulseTriangle,
     world::drawPulseQuad},
    {0x80042C70u,
     0x80042420u,
     0x80042698u,
     0x80042E84u,
     0x80042F08u,
     world::projectTriangle,
     world::projectQuadKeepFirst,
     world::drawLampTriangle,
     world::drawLampQuad},
    {0x80044A70u,
     0x80042420u,
     0x80042698u,
     0x80044E48u,
     0x80044EC0u,
     world::projectTriangle,
     world::projectQuadKeepFirst,
     world::drawGlowTriangle,
     world::drawGlowQuad},
    {0x80043B5Cu,
     0x80043B74u,
     0x80043C48u,
     0x80043C2Cu,
     0x80043D34u,
     world::projectWaveTriangle,
     world::projectWaveQuad,
     world::drawWaveTriangle,
     world::drawWaveQuad},
    {0x8004299Cu,
     0x80042AB8u,
     0x80042B7Cu,
     0x80042B44u,
     0x80042C2Cu,
     world::projectSwayTriangle,
     world::projectSwayQuad,
     world::drawSwayTriangle,
     world::drawSwayQuad},
    {0x80045C18u,
     0x80045D88u,
     0x80045F40u,
     0x80042548u,
     0x80042818u,
     world::projectLightTriangle,
     world::projectLightQuad,
     world::drawTriangle,
     world::drawQuad},
}};

const world::Mode &sceneMode(Core &core, std::uint32_t scene) {
  std::array<std::uint32_t, 5> entries{};
  for (std::uint32_t i = 0; i < entries.size(); ++i) {
    entries[i] = core.mem_r32(scene + kSceneModes + i * 4u);
  }
  for (const world::Mode &mode : kModes) {
    if (mode.setup == entries[0] && mode.projectTriangle == entries[1] && mode.projectQuad == entries[2] &&
        mode.drawTriangle == entries[3] && mode.drawQuad == entries[4]) {
      return mode;
    }
  }
  lucent::error("crash2-world",
                "unknown scene mode 0x{:08X} 0x{:08X} 0x{:08X} 0x{:08X} 0x{:08X}",
                entries[0],
                entries[1],
                entries[2],
                entries[3],
                entries[4]);
  std::abort();
}

// One corner of the record: its vertex's words (kept for the modes) and its GTE vector, offset by the world's.
struct Corner {
  std::uint32_t xy = 0;
  std::uint32_t z = 0;
};

Corner loadCorner(Pass &pass, int corner, const std::array<std::uint32_t, 4> &indices) {
  const std::uint32_t index = indices[corner];
  const std::uint32_t word = pass.word(pass.vertices - index * 4u - 4u);
  const std::uint32_t half = pass.core().mem_r16(pass.vertices + index * 2u);
  pass.words[corner] = word;
  pass.halves[corner] = half;
  return {.xy = ((word & world::kVertexMask) + pass.offsetXy) & world::kPairMask,
          .z = (half & kVertexZMask) + pass.offsetZ};
}

// 0x80042064 / 0x80042190: the polygon's corners into the GTE (a quad's fourth kept), then its mode's projector.
void loadPolygon(Pass &pass, const world::Mode &mode, std::uint32_t pointer) {
  const world::Record &record = pass.current;
  pass.material = (record.first << kMaterialShift) | (record.second & kMaterialLow);
  std::array<std::uint32_t, 4> indices{};
  int corners = 3;
  std::uint32_t rotated = 0;
  switch (record.kind) {
  case world::kKindTriangle:
    indices = {record.first >> 20, (record.first >> 8) & kCornerMask, (record.second >> 4) & kCornerMask, 0};
    pass.charge(run(0x80042064u, 0x80042148u));
    break;
  case world::kKindQuadFirst:
    indices = {record.first >> 20, (record.first >> 8) & kCornerMask, record.second >> 20, 0};
    pass.charge(run(0x80042190u, 0x800421A0u) + run(0x800421A4u, 0x80042250u) + run(0x8004214Cu, 0x8004218Cu));
    break;
  case world::kKindQuadLast:
    indices = {(record.first >> 8) & kCornerMask, record.second >> 20, (record.second >> 8) & kCornerMask, 0};
    rotated = world::kVertexMask;
    pass.charge(run(0x80042190u, 0x800421A0u) + run(0x800422B4u, 0x8004238Cu));
    break;
  default:
    indices = {
        record.first >> 20, (record.first >> 8) & kCornerMask, record.second >> 20, (record.second >> 8) & kCornerMask};
    corners = 4;
    pass.charge(run(0x80042190u, 0x800421A0u) + run(0x800421A4u, 0x80042250u) + run(0x80042254u, 0x800422B0u));
    break;
  }
  static constexpr std::array<std::uint32_t, 3> kVectors{psx::gte::kVxy0, psx::gte::kVxy1, psx::gte::kVxy2};
  Corner last;
  for (int corner = 0; corner < corners; ++corner) {
    last = loadCorner(pass, corner, indices);
    if (corner < 3) {
      pass.setGteData(kVectors[corner], last.xy);
      pass.setGteData(kVectors[corner] + 1u, last.z);
    }
  }
  pass.lastXy = last.xy;
  pass.lastZ = last.z;
  if (corners == 3) {
    pass.storeScratch(scratch::kRotated, rotated);
  }
  pass.upcomingEntry = pass.core().mem_r16(pointer);
  pass.ending = world::Ending::Skipped;
  if (corners == 4) {
    mode.projectQuadStep(pass);
  } else {
    mode.projectTriangleStep(pass);
  }
}

} // namespace

void WorldRenderer::draw(Core *core) {
  Core &c = *core;
  const std::uint32_t list = c.r[kA0];
  const std::uint32_t frame = c.r[kA1];
  const std::uint32_t scene = c.r[kA2];
  const std::uint32_t clock = c.r[kA3];
  const std::uint32_t returnAddress = c.r[kRa];
  const world::Mode &mode = sceneMode(c, scene);
  Pass pass(c, psx::present::recordCanvasColumns(c.game->guestDisplay.plan(), world::kScreenColumns));

  pass.charge(run(0x80041E5Cu, 0x80041ECCu));
  pass.storeScratch(scratch::kSavedSp, c.r[kSp] - kFrameSize);
  pass.setGteControl(psx::gte::kTranslationX, 0);
  pass.setGteControl(psx::gte::kTranslationY, 0);
  pass.setGteControl(psx::gte::kTranslationZ, 0);
  pass.storeScratch(scratch::kClock, clock);
  const auto count = static_cast<std::uint32_t>(pass.signedHalf(list));
  std::uint32_t pointer = list + 2u + count * 2u;
  const std::uint32_t first = list + kListFirst;
  pass.storeScratch(scratch::kFirst, first);
  pass.ot = frame + kFrameOt;
  if (pointer == first) {
    pass.charge(run(0x80041ED0u, 0x80041ED4u) + run(0x80041ED8u, 0x80041EE4u) + run(0x800423E4u, 0x8004241Cu));
    callGuestNow(c, kOwner, GuestCallSite{kResetOt, kResetOtReturn}, pass.ot, kOtSlots);
    c.r[kRa] = returnAddress;
    accountGuestInstructions(c, pass.charged());
    return;
  }

  pass.charge(run(0x80041EE8u, 0x80041F38u));
  const std::uint32_t camera = pass.word(scene);
  pass.storeScratch(scratch::kCameraX, world::sra(pass.word(camera), kCameraShift));
  pass.storeScratch(scratch::kCameraY, world::sra(pass.word(camera + 4u), kCameraShift));
  pass.storeScratch(scratch::kCameraZ, world::sra(pass.word(camera + 8u), kCameraShift));
  pass.storeScratch(scratch::kProjectTriangle, pass.word(scene + 0x14u));
  pass.storeScratch(scratch::kProjectQuad, pass.word(scene + 0x18u));
  pass.storeScratch(scratch::kDrawTriangle, pass.word(scene + 0x1Cu));
  pass.storeScratch(scratch::kDrawQuad, pass.word(scene + 0x20u));
  c.r[kV1] = scratch::kBase;
  callGuestNow(c, kOwner, GuestCallSite{mode.setup, kSetupReturn}, mode.setup, camera, scene, clock);
  c.r[kRa] = returnAddress;

  pass.charge(run(0x80041F3Cu, 0x80041FD8u));
  for (std::uint32_t i = 0; i < kDepthShiftWords; ++i) {
    pass.storeScratch(scratch::kDepthShifts + i * 4u, pass.word(kDepthShiftTable + i * 4u));
  }
  pass.next = pass.word(frame + kFrameNext);
  pass.storeScratch(scratch::kVertexMask, world::kVertexMask);
  pass.storeScratch(scratch::kPairMask, world::kPairMask);
  pass.storeScratch(scratch::kScreenEnd, world::kScreenEnd);
  pass.lowest = pass.ot + kOtTop;
  pass.store(pass.lowest, kOtEnd);
  pass.drawTriangleStep = mode.drawTriangleStep;
  pass.drawQuadStep = mode.drawQuadStep;

  pass.decode(c.mem_r16(pointer), pass.upcoming);
  pointer -= 2u;
  pass.charge(run(0x80042030u, 0x80042048u));
  pass.colours = pass.word(pass.upcoming.world + block::kColours);
  pass.storeScratch(scratch::kDrained, 0);
  while (true) {
    pass.current = pass.upcoming;
    pass.charge(run(0x8004204Cu, 0x80042060u));
    const std::uint32_t world = pass.current.world;
    pass.offsetXy = pass.word(world + block::kOffsetXy);
    pass.offsetZ = pass.word(world + block::kOffsetZ);
    pass.vertices = pass.word(world + block::kVertices);
    pass.animations = pass.word(world + block::kAnimations);
    pass.textures = pass.word(world + block::kTextures);
    loadPolygon(pass, mode, pointer);
    // A margin polygon ends as the retail cull ended it.
    const bool linked = pass.ending == world::Ending::Linked && !pass.margin();
    pass.endPolygon();
    pass.colours = pass.word(pass.upcoming.world + block::kColours);
    pass.charge(run(0x80042390u, 0x8004239Cu));
    const bool more = pointer != first;
    pointer -= 2u;
    if (more) {
      continue;
    }
    if (linked) {
      pass.charge(run(0x80042994u, 0x80042998u));
    }
    pass.charge(run(0x800423A0u, 0x800423ACu));
    const std::uint32_t drained = pass.scratchWord(scratch::kDrained);
    pass.storeScratch(scratch::kDrained, scratch::kBase);
    if (drained != 0u) {
      break;
    }
    pass.charge(run(0x800423B0u, 0x800423B4u));
    pointer += 2u;
  }

  pass.charge(run(0x800423B8u, 0x800423BCu));
  if (pass.lowest != pass.ot) {
    pass.charge(run(0x800423C0u, 0x800423C8u));
    pass.linkDown(pass.ot - 4u);
  }
  pass.charge(run(0x800423DCu, 0x8004241Cu));
  pass.store(frame + kFrameNext, pass.next);
  accountGuestInstructions(c, pass.charged());
}

void WorldRenderer::registerOverrides(Core &core) {
  installNativeOverride(core, kDrawWorlds, "Crash 2 Gfx_DrawWorlds", draw, NativeReturn::Void);
}

} // namespace crash2
