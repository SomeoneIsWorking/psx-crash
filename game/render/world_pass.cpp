#include "render/world_pass.h"

#include "gte_registers.h"

namespace crash::world {
namespace {

inline constexpr std::uint32_t kEntryWorldMask = 0xE000u;
inline constexpr std::uint32_t kEntryWorldShift = 8u;
inline constexpr std::uint32_t kEntryIndexMask = 0x7FFu;
inline constexpr std::uint32_t kQuadRecordBytes = 8u;
inline constexpr std::uint32_t kMaterialAnimated = 0x800u;
inline constexpr std::uint32_t kMaterialIndexMask = 0x7FFu;
inline constexpr std::uint32_t kTextureBytes = 12u;
inline constexpr std::uint32_t kDepthShiftStep = 6u;
inline constexpr std::uint32_t kAverageShift = 15u;
inline constexpr std::uint32_t kSumShift = 5u;
inline constexpr std::uint32_t kLevelMask = 3u;
inline constexpr std::uint32_t kShiftMask = 31u;
inline constexpr std::uint32_t kTagLengthShift = 24u;
inline constexpr std::uint32_t kLinkMask = 0x00FFFFFFu;
inline constexpr std::uint32_t kHalfLow = 0xFFFFu;
inline constexpr std::uint32_t kHalfHigh = 0xFFFF0000u;

} // namespace

void Pass::setFarColour(std::uint32_t red, std::uint32_t green, std::uint32_t blue) {
  gte_.writeControl(psx::gte::kFarRed, red);
  gte_.writeControl(psx::gte::kFarGreen, green);
  gte_.writeControl(psx::gte::kFarBlue, blue);
}

Cull Pass::offScreen(const std::uint32_t *screens, int corners) {
  if (title_.averageDepth) {
    gte_.execute(corners == 3 ? psx::gte::kAvsz3 : psx::gte::kAvsz4);
  }
  const ScreenVerdict verdict = screen_.test(screens, corners);
  if (verdict.cull == Cull::Rows) {
    return Cull::Rows;
  }
  charge(2);
  margin_ = verdict.margin;
  return verdict.cull;
}

std::uint32_t Pass::vertexColour(int corner) {
  const std::uint32_t vertex = words[corner];
  const std::uint32_t offset = ((halves[corner] << 2) & 0x3Cu) | ((vertex >> 6) & 0xC00u) | ((vertex << 6) & 0x3C0u);
  return core_.mem_r32(colours + offset);
}
void Pass::decode(std::uint32_t entry, Record &record) {
  const std::uint32_t world = entry & kEntryWorldMask;
  if (title_.storesEntryWorld) {
    storeScratch(scratch::kEntryWorld, world);
  }
  record.world = scratch::kBase + scratch::kWorlds + (world >> kEntryWorldShift);
  record.kind = entry & kKindMask;
  const std::uint32_t index = entry & kEntryIndexMask;
  if (record.kind != kKindTriangle) {
    const std::uint32_t quad = core_.mem_r32(record.world + block::kQuads) + index * kQuadRecordBytes;
    record.first = core_.mem_r32(quad);
    record.second = core_.mem_r32(quad + 4u);
    charge(costs().decodeQuad);
    return;
  }
  const std::uint32_t triangles = core_.mem_r32(record.world + block::kTriangles);
  record.second = core_.mem_r16(triangles + index * 2u);
  record.first = core_.mem_r32(triangles - index * 4u - 4u);
  charge(costs().decodeTriangle);
}

void Pass::decodeNext() {
  decode(upcomingEntry, upcoming);
}

std::uint32_t Pass::depthSlot() {
  if (title_.averageDepth) {
    depth = gte_.readData(psx::gte::kMac0) >> kAverageShift;
  } else {
    const std::uint32_t sum =
        gte_.readData(psx::gte::kSz1) + gte_.readData(psx::gte::kSz2) + gte_.readData(psx::gte::kSz3);
    depth = sum >> kSumShift;
  }
  auto index = static_cast<std::int32_t>(kDeepest - depth);
  std::uint32_t clamped = 0;
  if (index < 0) {
    index = 0;
    clamped = 1;
  }
  slot = static_cast<std::uint32_t>(index) * 4u + ot;
  return clamped;
}

TextureLookup Pass::lookupTexture() {
  std::uint32_t index = material & kMaterialIndexMask;
  if ((material & kMaterialAnimated) == 0u) {
    return {.address = textures + index * kTextureBytes - kTextureBytes, .path = TexturePath::Fixed};
  }
  std::uint32_t animation = core_.mem_r32(animations + index * 4u);
  const std::uint32_t levelShift =
      title_.sceneLevelShift != 0u ? scratchWord(scratch::kLevelShift) & kShiftMask : kDepthShiftStep;
  const std::uint32_t shift = core_.mem_r8(scratch::kBase + scratch::kDepthShifts + (depth >> levelShift));
  TexturePath path = TexturePath::Level;
  if (!negative(animation)) {
    const std::uint32_t clock = scratchWord(scratch::kClock);
    std::uint32_t frame = (clock >> ((animation >> 26) & kShiftMask)) + (animation >> 19);
    frame &= (animation >> 12) & 0x7Fu;
    index = (animation + frame) & kMaterialIndexMask;
    if ((animation & kMaterialAnimated) == 0u) {
      return {.address = textures + index * kTextureBytes - kTextureBytes, .path = TexturePath::Frame};
    }
    animation = core_.mem_r32(animations + index * 4u);
    path = TexturePath::FrameLevel;
  }
  index = (animation & kMaterialIndexMask) + ((animation >> (shift & kShiftMask)) & kLevelMask);
  return {.address = textures + index * kTextureBytes - kTextureBytes, .path = path};
}

std::uint32_t Pass::textureAddress() {
  const TextureLookup lookup = lookupTexture();
  const Costs &cost = costs();
  switch (lookup.path) {
  case TexturePath::Fixed:
    charge(cost.textureFixed);
    break;
  case TexturePath::Frame:
    charge(cost.textureHead + cost.textureAnimation + cost.textureEnd);
    break;
  case TexturePath::FrameLevel:
    charge(cost.textureHead + cost.textureAnimation + cost.textureFrameLevel + cost.textureLevel + cost.textureEnd);
    break;
  case TexturePath::Level:
    charge(cost.textureHead + cost.textureLevel + cost.textureEnd);
    break;
  }
  return lookup.address;
}

std::array<std::uint32_t, 3> Pass::textureWords(std::uint32_t texture) {
  std::array<std::uint32_t, 3> corners{
      core_.mem_r32(texture), core_.mem_r32(texture + 4u), core_.mem_r32(texture + 8u)};
  if (scratchWord(scratch::kRotated) != 0u) {
    corners = {(corners[0] & kHalfHigh) | (corners[1] & kHalfLow),
               (corners[1] & kHalfHigh) | (corners[2] & kHalfLow),
               corners[2] >> 16};
  }
  return corners;
}

std::array<std::uint32_t, 3> Pass::triangleTexture() {
  return textureWords(textureAddress());
}

void Pass::linkPrimitive(std::uint32_t words) {
  switch (link) {
  case Link::Sorted:
    linkSorted(words);
    break;
  case Link::Bucket:
    linkBucket(words);
    break;
  case Link::OwnSlot:
    linkOwnSlot(words);
    break;
  }
  ending = Ending::Linked;
}

void Pass::linkSorted(std::uint32_t words) {
  charge(costs().linkHead);
  if (static_cast<std::int32_t>(slot - lowest) < 0) {
    charge(costs().relinkHead + relinkDown(slot) * costs().relinkSlot);
  }
  charge(costs().linkAppend);
  appendAtLowest(words);
}

void Pass::linkBucket(std::uint32_t words) {
  charge(costs().bucketHead);
  if (scratchWord(scratch::kEntryWorld) == 0u) {
    charge(costs().bucketFront);
    linkAtHead(core_, ot, next, words);
    return;
  }
  charge(costs().bucketCompare);
  if (static_cast<std::int32_t>(slot - lowest) < 0) {
    charge(costs().bucketRaise);
    lowest = slot;
  }
  charge(costs().bucketAppend);
  linkAtHead(core_, lowest, next, words);
}

void Pass::linkOwnSlot(std::uint32_t words) {
  charge(costs().ownSlot);
  linkAtHead(core_, slot, next, words);
}

std::uint32_t Pass::relinkDown(std::uint32_t target) {
  std::uint32_t link = lowest & kLinkMask;
  std::uint32_t stored = 0;
  lowest -= 4u;
  while (target != lowest) {
    core_.mem_w32(lowest, link);
    lowest -= 4u;
    link -= 4u;
    ++stored;
  }
  head = link;
  return stored;
}

void Pass::appendAtLowest(std::uint32_t words) {
  core_.mem_w32(next, head | ((words - 1u) << kTagLengthShift));
  head = next & kLinkMask;
  core_.mem_w32(lowest, head);
  next += words * 4u;
}

void Pass::linkDown(std::uint32_t past) {
  std::uint32_t link = lowest & kLinkMask;
  lowest -= 4u;
  while (true) {
    core_.mem_w32(lowest, link);
    lowest -= 4u;
    link -= 4u;
    charge(costs().linkDownSlot);
    if (lowest == past) {
      break;
    }
  }
}

} // namespace crash::world
