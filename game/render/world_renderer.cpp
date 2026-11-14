#include "render/world_renderer.h"

#include "core.h"
#include "execution_services.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_call.h"
#include "r3000.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace crash::world {
namespace {

using namespace psx::cpu;

inline constexpr std::uint32_t kDepthShiftWords = 8u;
inline constexpr std::uint32_t kFrameSize = 0x44u;
inline constexpr std::uint32_t kFrameNext = 0x08u;
inline constexpr std::uint32_t kOtSlots = 0x800u;
inline constexpr std::uint32_t kOtTop = 0x1FFCu;
inline constexpr std::uint32_t kOtEnd = 0x00FFFFFFu;
inline constexpr std::uint32_t kListFirst = 4u;
inline constexpr std::uint32_t kVertexZMask = 0xFFF0u;
inline constexpr std::uint32_t kCornerMask = 0xFFFu;
inline constexpr std::uint32_t kMaterialShift = 4u;
inline constexpr std::uint32_t kMaterialLow = 0xFu;
inline constexpr std::uint32_t kCameraShift = 8u;
// The scene's four steps after its setup word.
inline constexpr std::uint32_t kStepWords = 4u;

const Mode &sceneMode(Core &core, const Title &title, std::uint32_t scene) {
  std::array<std::uint32_t, 1 + kStepWords> entries{};
  for (std::uint32_t i = 0; i < entries.size(); ++i) {
    entries[i] = core.mem_r32(scene + title.sceneModes + i * 4u);
  }
  for (const Mode &mode : title.modes) {
    if (mode.setup == entries[0] && mode.projectTriangle == entries[1] && mode.projectQuad == entries[2] &&
        mode.drawTriangle == entries[3] && mode.drawQuad == entries[4]) {
      return mode;
    }
  }
  lucent::error("crash-world",
                "{}: unknown scene mode 0x{:08X} 0x{:08X} 0x{:08X} 0x{:08X} 0x{:08X}",
                title.owner,
                entries[0],
                entries[1],
                entries[2],
                entries[3],
                entries[4]);
  std::abort();
}

Link sceneLink(Core &core, const Title &title, std::uint32_t scene) {
  if (title.sceneLinkTail == 0u) {
    return Link::Sorted;
  }
  const std::uint32_t tail = core.mem_r32(scene + title.sceneLinkTail);
  for (const LinkTail &known : title.tails) {
    if (known.address == tail) {
      return known.link;
    }
  }
  lucent::error("crash-world", "{}: unknown link tail 0x{:08X}", title.owner, tail);
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
  const std::uint32_t shift = pass.title().vertexShift;
  return {.xy = (((word & kVertexMask) >> shift) + pass.offsetXy) & kPairMask,
          .z = ((half & kVertexZMask) >> shift) + pass.offsetZ};
}

// The polygon's corners into the GTE (a quad's fourth kept), then its mode's projector.
void loadPolygon(Pass &pass, const Mode &mode, std::uint32_t pointer) {
  const Record &record = pass.current;
  const Costs &costs = pass.costs();
  pass.material = (record.first << kMaterialShift) | (record.second & kMaterialLow);
  std::array<std::uint32_t, 4> indices{};
  int corners = 3;
  std::uint32_t rotated = 0;
  switch (record.kind) {
  case kKindTriangle:
    indices = {record.first >> 20, (record.first >> 8) & kCornerMask, (record.second >> 4) & kCornerMask, 0};
    pass.charge(costs.loadTriangle);
    break;
  case kKindQuadFirst:
    indices = {record.first >> 20, (record.first >> 8) & kCornerMask, record.second >> 20, 0};
    pass.charge(costs.loadQuadFirst);
    break;
  case kKindQuadLast:
    indices = {(record.first >> 8) & kCornerMask, record.second >> 20, (record.second >> 8) & kCornerMask, 0};
    rotated = kVertexMask;
    pass.charge(costs.loadQuadLast);
    break;
  default:
    indices = {
        record.first >> 20, (record.first >> 8) & kCornerMask, record.second >> 20, (record.second >> 8) & kCornerMask};
    corners = 4;
    pass.charge(costs.loadQuad);
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
  pass.ending = Ending::Skipped;
  if (corners == 4) {
    mode.projectQuadStep(pass);
  } else {
    mode.projectTriangleStep(pass);
  }
}

} // namespace

void drawWorlds(Core &c, const Title &title) {
  const std::uint32_t list = c.r[kA0];
  const std::uint32_t frame = c.r[kA1];
  const std::uint32_t scene = c.r[kA2];
  const std::uint32_t clock = c.r[kA3];
  const std::uint32_t returnAddress = c.r[kRa];
  const Mode &mode = sceneMode(c, title, scene);
  Pass pass(c, title, psx::present::recordCanvasColumns(c.game->guestDisplay.plan(), kScreenColumns));
  const Costs &costs = title.costs;

  pass.charge(costs.prologue);
  pass.storeScratch(scratch::kSavedSp, c.r[kSp] - kFrameSize);
  pass.setGteControl(psx::gte::kTranslationX, 0);
  pass.setGteControl(psx::gte::kTranslationY, 0);
  pass.setGteControl(psx::gte::kTranslationZ, 0);
  if (title.averageDepth) {
    pass.setGteControl(psx::gte::kZsf3, title.averageScale3);
    pass.setGteControl(psx::gte::kZsf4, title.averageScale4);
  }
  pass.storeScratch(scratch::kClock, clock);
  const auto count = static_cast<std::uint32_t>(pass.signedHalf(list));
  std::uint32_t pointer = list + 2u + count * 2u;
  const std::uint32_t first = list + kListFirst;
  pass.storeScratch(scratch::kFirst, first);
  pass.ot = frame + title.frameOt;
  if (pointer == first) {
    pass.charge(costs.emptyList);
    callGuestNow(c, title.owner, GuestCallSite{title.resetOt, title.resetOtReturn}, pass.ot, kOtSlots);
    c.r[kRa] = returnAddress;
    accountGuestInstructions(c, pass.charged());
    return;
  }

  pass.charge(costs.beforeSetup);
  const std::uint32_t camera = pass.word(scene);
  pass.storeScratch(scratch::kCameraX, sra(pass.word(camera), kCameraShift));
  pass.storeScratch(scratch::kCameraY, sra(pass.word(camera + 4u), kCameraShift));
  pass.storeScratch(scratch::kCameraZ, sra(pass.word(camera + 8u), kCameraShift));
  const std::uint32_t steps = scene + title.sceneModes + 4u;
  pass.storeScratch(scratch::kProjectTriangle, pass.word(steps));
  pass.storeScratch(scratch::kProjectQuad, pass.word(steps + 4u));
  pass.storeScratch(scratch::kDrawTriangle, pass.word(steps + 8u));
  pass.storeScratch(scratch::kDrawQuad, pass.word(steps + 12u));
  if (title.sceneLinkTail != 0u) {
    pass.storeScratch(scratch::kLinkTail, pass.word(scene + title.sceneLinkTail));
  }
  if (title.sceneLevelShift != 0u) {
    pass.storeScratch(scratch::kLevelShift, pass.word(scene + title.sceneLevelShift));
  }
  pass.link = sceneLink(c, title, scene);
  c.r[kV1] = scratch::kBase;
  callGuestNow(c, title.owner, GuestCallSite{mode.setup, title.setupReturn}, mode.setup, camera, scene, clock);
  c.r[kRa] = returnAddress;

  pass.charge(costs.afterSetup);
  for (std::uint32_t i = 0; i < kDepthShiftWords; ++i) {
    pass.storeScratch(scratch::kDepthShifts + i * 4u, pass.word(title.depthShiftTable + i * 4u));
  }
  pass.next = pass.word(frame + kFrameNext);
  pass.storeScratch(scratch::kVertexMask, kVertexMask);
  pass.storeScratch(scratch::kPairMask, kPairMask);
  pass.storeScratch(scratch::kScreenEnd, kScreenEnd);
  pass.lowest = pass.ot + kOtTop;
  pass.store(pass.lowest, kOtEnd);
  pass.drawTriangleStep = mode.drawTriangleStep;
  pass.drawQuadStep = mode.drawQuadStep;

  pass.decode(c.mem_r16(pointer), pass.upcoming);
  pointer -= 2u;
  pass.charge(costs.firstDecode);
  pass.colours = pass.word(pass.upcoming.world + block::kColours);
  pass.storeScratch(scratch::kDrained, 0);
  while (true) {
    pass.current = pass.upcoming;
    pass.charge(costs.loopTop);
    const std::uint32_t world = pass.current.world;
    pass.offsetXy = pass.word(world + block::kOffsetXy);
    pass.offsetZ = pass.word(world + block::kOffsetZ);
    pass.vertices = pass.word(world + block::kVertices);
    pass.animations = pass.word(world + block::kAnimations);
    pass.textures = pass.word(world + block::kTextures);
    loadPolygon(pass, mode, pointer);
    // A margin polygon ends as the retail cull ended it.
    const bool linked = pass.ending == Ending::Linked && !pass.margin();
    pass.endPolygon();
    pass.colours = pass.word(pass.upcoming.world + block::kColours);
    pass.charge(costs.nextEntry);
    const bool more = pointer != first;
    pointer -= 2u;
    if (more) {
      continue;
    }
    if (linked) {
      pass.charge(costs.linkedFinish);
    }
    pass.charge(costs.drain);
    const std::uint32_t drained = pass.scratchWord(scratch::kDrained);
    pass.storeScratch(scratch::kDrained, scratch::kBase);
    if (drained != 0u) {
      break;
    }
    pass.charge(costs.drainAgain);
    pointer += 2u;
  }

  if (pass.link == Link::Sorted) {
    pass.charge(costs.afterLoop);
    if (pass.lowest != pass.ot) {
      pass.charge(costs.linkDownHead);
      pass.linkDown(pass.ot - 4u);
    }
  } else {
    pass.charge(costs.afterLoopUnsorted);
  }
  pass.charge(costs.epilogue);
  pass.store(frame + kFrameNext, pass.next);
  accountGuestInstructions(c, pass.charged());
}

} // namespace crash::world
