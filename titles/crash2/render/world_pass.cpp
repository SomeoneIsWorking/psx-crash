#include "crash2/render/world_pass.h"

#include "gte_registers.h"

namespace crash2::world {
namespace {

inline constexpr std::uint32_t kEntryWorldMask = 0xE000u;
inline constexpr std::uint32_t kEntryWorldShift = 8u;
inline constexpr std::uint32_t kEntryIndexMask = 0x7FFu;
inline constexpr std::uint32_t kQuadRecordBytes = 8u;
inline constexpr std::uint32_t kMaterialAnimated = 0x800u;
inline constexpr std::uint32_t kMaterialIndexMask = 0x7FFu;
inline constexpr std::uint32_t kTextureBytes = 12u;
inline constexpr std::uint32_t kDepthShiftStep = 6u;
inline constexpr std::uint32_t kLevelMask = 3u;
inline constexpr std::uint32_t kShiftMask = 31u;
inline constexpr std::uint32_t kTagLengthShift = 24u;
inline constexpr std::uint32_t kLinkMask = 0x00FFFFFFu;
inline constexpr std::uint32_t kHalfLow = 0xFFFFu;
inline constexpr std::uint32_t kHalfHigh = 0xFFFF0000u;
inline constexpr std::uint32_t kLaneSign = 0x8000u;

} // namespace

void Pass::setFarColour(std::uint32_t red, std::uint32_t green, std::uint32_t blue) {
  gte_.writeControl(psx::gte::kFarRed, red);
  gte_.writeControl(psx::gte::kFarGreen, green);
  gte_.writeControl(psx::gte::kFarBlue, blue);
}

Cull Pass::offScreen(const std::uint32_t *screens, int corners) {
  // Rows: the retail packed test's high lane (the low lane's borrow included), against the retail bound alone.
  std::uint32_t past = 0;
  std::uint32_t all = ~0u;
  // Columns: every corner right of the last column, or every corner left of the first.
  std::uint32_t right = 0;
  std::uint32_t left = kLaneSign;
  std::uint32_t retailRight = 0;
  std::uint32_t retailLeft = kLaneSign;
  const auto lastColumn = static_cast<std::uint32_t>(columns_.right);
  const auto firstColumn = static_cast<std::uint32_t>(columns_.left);
  for (int corner = 0; corner < corners; ++corner) {
    const std::uint32_t point = screens[corner];
    past |= point - kScreenEnd;
    all &= point;
    right |= (point - lastColumn) & kLaneSign;
    left &= (point - firstColumn) & kLaneSign;
    retailRight |= (point - static_cast<std::uint32_t>(kScreenColumns)) & kLaneSign;
    retailLeft &= point & kLaneSign;
  }
  if (negative(~past | all)) {
    return Cull::Rows;
  }
  charge(2);
  if (right == 0u || left != 0u) {
    return Cull::Columns;
  }
  margin_ = retailRight == 0u || retailLeft != 0u;
  return Cull::Visible;
}

std::uint32_t Pass::vertexColour(int corner) {
  const std::uint32_t vertex = words[corner];
  const std::uint32_t offset = ((halves[corner] << 2) & 0x3Cu) | ((vertex >> 6) & 0xC00u) | ((vertex << 6) & 0x3C0u);
  return core_.mem_r32(colours + offset);
}

void Pass::decode(std::uint32_t entry, Record &record) {
  record.world = scratch::kBase + scratch::kWorlds + ((entry & kEntryWorldMask) >> kEntryWorldShift);
  record.kind = entry & kKindMask;
  const std::uint32_t index = entry & kEntryIndexMask;
  if (record.kind != kKindTriangle) {
    const std::uint32_t quad = core_.mem_r32(record.world + block::kQuads) + index * kQuadRecordBytes;
    record.first = core_.mem_r32(quad);
    record.second = core_.mem_r32(quad + 4u);
    charge(run(0x80042440u, 0x80042464u));
    return;
  }
  const std::uint32_t triangles = core_.mem_r32(record.world + block::kTriangles);
  record.second = core_.mem_r16(triangles + index * 2u);
  record.first = core_.mem_r32(triangles - index * 4u - 4u);
  charge(run(0x80042468u, 0x80042490u));
}

void Pass::decodeNext() {
  decode(upcomingEntry, upcoming);
}

std::uint32_t Pass::depthSlot() {
  const std::uint32_t sum =
      gte_.readData(psx::gte::kSz1) + gte_.readData(psx::gte::kSz2) + gte_.readData(psx::gte::kSz3);
  depth = sum >> 5;
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
  const std::uint32_t shift = core_.mem_r8(scratch::kBase + scratch::kDepthShifts + (depth >> kDepthShiftStep));
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
  switch (lookup.path) {
  case TexturePath::Fixed:
    charge(run(0x8004258Cu, 0x80042598u));
    break;
  case TexturePath::Frame:
    charge(run(0x8004259Cu, 0x800425B4u) + run(0x800425B8u, 0x800425E4u) + run(0x80042608u, 0x80042610u));
    break;
  case TexturePath::FrameLevel:
    charge(run(0x8004259Cu, 0x800425B4u) + run(0x800425B8u, 0x800425E4u) + run(0x800425E8u, 0x800425F4u) +
           run(0x800425F8u, 0x80042604u) + run(0x80042608u, 0x80042610u));
    break;
  case TexturePath::Level:
    charge(run(0x8004259Cu, 0x800425B4u) + run(0x800425F8u, 0x80042604u) + run(0x80042608u, 0x80042610u));
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
  charge(run(0x80042948u, 0x80042950u));
  if (static_cast<std::int32_t>(slot - lowest) < 0) {
    charge(run(0x80042954u, 0x80042960u) + relinkDown(slot) * run(0x80042964u, 0x80042970u));
  }
  charge(run(0x80042974u, 0x80042980u));
  appendAtLowest(words);
  ending = Ending::Linked;
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
    charge(run(0x800423CCu, 0x800423D8u));
    if (lowest == past) {
      break;
    }
  }
}

} // namespace crash2::world
