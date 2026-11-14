// The light around Crash (Gfx_SetupScene with game flag 0x1000000): a polygon with a bit 18 vertex near the light is
// divided into a nine-row grid lit from the scene's height and shade maps and drawn as strips, over itself drawn
// farther back (crash2-decomp docs/notes/world-renderer.md, "Light").
#include "crash2/render/world_steps.h"

#include "gte_registers.h"

namespace crash2::world {
namespace {

using psx::gte::kDcpl;
using psx::gte::kRtps;
using psx::gte::kRtpt;

// Scratchpad words of the light walk.
inline constexpr std::uint32_t kGridX = 0x110u;
inline constexpr std::uint32_t kGridY = 0x11Cu;
inline constexpr std::uint32_t kGridZ = 0x128u;
inline constexpr std::uint32_t kGridColour = 0x134u;
inline constexpr std::uint32_t kGridLight = 0x14Cu;
inline constexpr std::uint32_t kGridTexture = 0x15Cu;
inline constexpr std::uint32_t kHeights = 0x170u;
inline constexpr std::uint32_t kLightCorners = 0x190u;
inline constexpr std::uint32_t kLightReturn = 0x18Cu;
inline constexpr std::uint32_t kLightColours = 0x1A8u;
inline constexpr std::uint32_t kLightVectors = 0x1BCu;
inline constexpr std::uint32_t kShades = 0x1CCu;
inline constexpr std::uint32_t kLightDepth = 0x1D0u;
inline constexpr std::uint32_t kLightGrid = 0x1D4u;
inline constexpr std::uint32_t kLightSlot = 0x1E0u;
inline constexpr std::uint32_t kSavedTexture = 0x1E4u;
inline constexpr std::uint32_t kMaterial = 0x218u;
inline constexpr std::uint32_t kLastXy = 0x220u;
inline constexpr std::uint32_t kLastZ = 0x224u;
inline constexpr std::uint32_t kFirstColour = 0x228u;

// The strip list: grid indices, a set top bit starting a strip, -1 ending the list.
inline constexpr std::uint32_t kStrips = 0x8005C6D0u;
inline constexpr std::uint32_t kGridPointBytes = 12u;
inline constexpr std::uint32_t kGridRows = 8u;
inline constexpr std::uint32_t kNearMask = 0xFC00u;
inline constexpr std::uint32_t kMapMask = 0xFFF0u;
inline constexpr std::uint32_t kNoHeight = 0xFFu;
inline constexpr std::uint32_t kLaneCarry = 0xFFFF7FFFu;
inline constexpr std::uint32_t kDeltaMask = 0x7FFFu;
inline constexpr std::uint32_t kFarther = 0xA0u;
inline constexpr std::uint32_t kPolyG3 = 0x30000000u;
inline constexpr std::uint32_t kPolyGt3 = 0x34000000u;

std::uint32_t word(Pass &pass, std::uint32_t offset) {
  return pass.scratchWord(offset);
}
void setWord(Pass &pass, std::uint32_t offset, std::uint32_t value) {
  pass.storeScratch(offset, value);
}
std::uint32_t half(Pass &pass, std::uint32_t offset) {
  return static_cast<std::uint32_t>(pass.signedHalf(scratch::kBase + offset));
}
void setHalf(Pass &pass, std::uint32_t offset, std::uint32_t value) {
  pass.storeHalf(scratch::kBase + offset, value);
}
std::uint32_t byte(Pass &pass, std::uint32_t address) {
  return pass.core().mem_r8(address);
}
std::uint32_t lowHalf(std::uint32_t value) {
  return sra(value << 16, 16);
}

// 0x80045D9C: some corner within the light's reach along z, and some along x.
// The light's two lanes: its z against VZ, its x against VXY's low half.
enum class LightLane : std::uint8_t {
  Z,
  X,
};

// The first corner near the light along `lane`, 3 when none is.
std::uint32_t nearCorner(Pass &pass, LightLane lane) {
  const bool alongZ = lane == LightLane::Z;
  const std::uint32_t light = word(pass, alongZ ? scratch::kLightZ : scratch::kLightX);
  const std::uint32_t firstVector = alongZ ? psx::gte::kVz0 : psx::gte::kVxy0;
  pass.charge(run(0x80045D9Cu, 0x80045DB4u));
  for (std::uint32_t corner = 0; corner < 3; ++corner) {
    if (corner > 0) {
      pass.charge(run(0x80045DB8u, 0x80045DC0u));
    }
    if (((light - pass.gteData(firstVector + corner * 2u)) & kNearMask) == 0u) {
      return corner;
    }
  }
  return 3;
}

bool nearLight(Pass &pass) {
  const std::uint32_t alongZ = nearCorner(pass, LightLane::Z);
  if (alongZ == 3u) {
    return false;
  }
  if (alongZ < 2u) {
    // The branch to the x test lands on the delay slot's load.
    pass.charge(1);
  }
  return nearCorner(pass, LightLane::X) != 3u;
}

bool markedForLight(const Pass &pass, int corners) {
  std::uint32_t any = 0;
  for (int corner = 0; corner < corners; ++corner) {
    any |= pass.words[corner];
  }
  return (any & kBit18) != 0u;
}

// A grid point (or corner) of a lit triangle: its colour, the word holding its screen position, and its depth.
struct LitPoint {
  std::uint32_t colour = 0;
  std::uint32_t screen = 0;
  std::uint32_t depth = 0;
};

// 0x80045C5C / 0x80045CAC: a lit triangle at `slot`, or (own slot) at its own depth less 5 unless a corner was unlit.
void emitLit(Pass &pass, const std::array<LitPoint, 3> &points, bool textured, bool ownSlot, std::uint32_t slot) {
  if (ownSlot) {
    pass.charge(run(0x80045C5Cu, 0x80045C64u));
    if (negative(points[0].colour | points[1].colour | points[2].colour)) {
      pass.charge(run(0x80045C6Cu, 0x80045C70u));
      return;
    }
    pass.charge(run(0x80045C74u, 0x80045C98u) + run(0x80045CA0u, 0x80045CA4u));
    const std::uint32_t sum = lowHalf(points[0].depth + points[1].depth + points[2].depth);
    std::uint32_t index = kDeepest - (sum >> 5) - 5u;
    if (!negative(index - kDeepest)) {
      pass.charge(1);
      index = kDeepest * 4u;
    } else {
      index <<= 2;
    }
    if (negative(index)) {
      pass.charge(1);
      slot = pass.ot;
    } else {
      slot = index + pass.ot;
    }
  }
  const std::uint32_t primitive = pass.next;
  std::uint32_t words = 0;
  pass.charge(run(0x80045CACu, 0x80045CB0u));
  if (!textured) {
    pass.charge(run(0x80045CB4u, 0x80045CD8u));
    pass.store(primitive + 0x04u, points[0].colour | kPolyG3);
    pass.store(primitive + 0x0Cu, points[1].colour);
    pass.store(primitive + 0x14u, points[2].colour);
    pass.copyScreen(primitive + 0x08u, points[0].screen);
    pass.copyScreen(primitive + 0x10u, points[1].screen);
    pass.copyScreen(primitive + 0x18u, points[2].screen);
    words = 7;
  } else {
    pass.charge(run(0x80045CDCu, 0x80045D18u));
    pass.store(primitive + 0x0Cu, word(pass, scratch::kLightTexture));
    pass.store(primitive + 0x18u, word(pass, scratch::kLightTexture + 4u));
    pass.store(primitive + 0x24u, word(pass, scratch::kLightTexture + 8u));
    pass.copyScreen(primitive + 0x08u, points[0].screen);
    pass.copyScreen(primitive + 0x14u, points[1].screen);
    pass.copyScreen(primitive + 0x20u, points[2].screen);
    pass.store(primitive + 0x04u, points[0].colour | kPolyGt3);
    pass.store(primitive + 0x10u, points[1].colour);
    pass.store(primitive + 0x1Cu, points[2].colour);
    words = 10;
  }
  pass.charge(run(0x80045D1Cu, 0x80045D20u));
  const bool above = !negative(slot - pass.lowest);
  if (ownSlot) {
    pass.charge(run(0x80045D24u, 0x80045D28u));
    if (above) {
      // Put at the head of its own slot, the lowest left as it was.
      pass.charge(run(0x80045D2Cu, 0x80045D44u));
      pass.store(primitive, pass.word(slot) | ((words - 1u) << 24));
      pass.store(slot, primitive & 0x00FFFFFFu);
      pass.next += words * 4u;
      return;
    }
  }
  pass.charge(run(0x80045D48u, 0x80045D4Cu));
  if (!above) {
    pass.charge(run(0x80045D50u, 0x80045D5Cu) + pass.relinkDown(slot) * run(0x80045D60u, 0x80045D6Cu));
  }
  pass.charge(run(0x80045D70u, 0x80045D84u));
  pass.appendAtLowest(words);
}

// One lane of the grid: its start (twice: the row's and the point's) and its steps along the two edges.
void startLane(Pass &pass, std::uint32_t offset, std::uint32_t start, std::uint32_t first, std::uint32_t second) {
  setHalf(pass, offset, start);
  setHalf(pass, offset + 4u, start);
  setHalf(pass, offset + 8u, first);
  setHalf(pass, offset + 0xCu, second);
}

// 0x800463E4: the texture the walk's grid and strips take, kept at 0xF4 and 0x1E4, and its corners' u and v.
void walkTexture(Pass &pass) {
  const TextureLookup lookup = pass.lookupTexture();
  switch (lookup.path) {
  case TexturePath::Fixed:
    pass.charge(run(0x800463E4u, 0x800463FCu));
    break;
  case TexturePath::Frame:
    pass.charge(run(0x800463E4u, 0x800463ECu) + run(0x80046400u, 0x80046418u) + run(0x8004641Cu, 0x80046448u) +
                run(0x8004646Cu, 0x80046474u));
    break;
  case TexturePath::FrameLevel:
    pass.charge(run(0x800463E4u, 0x800463ECu) + run(0x80046400u, 0x80046418u) + run(0x8004641Cu, 0x80046448u) +
                run(0x8004644Cu, 0x80046458u) + run(0x8004645Cu, 0x80046474u));
    break;
  case TexturePath::Level:
    pass.charge(run(0x800463E4u, 0x800463ECu) + run(0x80046400u, 0x80046418u) + run(0x8004645Cu, 0x80046474u));
    break;
  }
  const bool rotated = word(pass, scratch::kRotated) != 0u;
  pass.charge(run(0x80046478u, 0x8004648Cu) + (rotated ? run(0x80046490u, 0x800464B0u) : 0u) +
              run(0x800464B4u, 0x80046520u));
  const std::array<std::uint32_t, 3> texture = pass.textureWords(lookup.address);
  for (std::uint32_t i = 0; i < 3; ++i) {
    setWord(pass, scratch::kLightTexture + i * 4u, texture[i]);
    setWord(pass, kSavedTexture + i * 4u, texture[i]);
  }
  // u's steps lose bit 15, v's keep it.
  for (std::uint32_t lane = 0; lane < 2; ++lane) {
    const std::uint32_t mask = lane == 0u ? kDeltaMask : 0xFFFFu;
    const std::uint32_t first = byte(pass, scratch::kBase + scratch::kLightTexture + lane);
    const std::uint32_t second = byte(pass, scratch::kBase + scratch::kLightTexture + 4u + lane);
    const std::uint32_t third = byte(pass, scratch::kBase + scratch::kLightTexture + 8u + lane);
    startLane(pass, kGridTexture + lane * 2u, first << 3, (second - first) & mask, (third - first) & mask);
  }
}

// 0x80046540: one grid point lit from the maps under the light and projected; left -1 off the maps.
void litPoint(Pass &pass, std::uint32_t point, bool textured) {
  pass.store(point, 0xFFFFFFFFu);
  const std::uint32_t along = half(pass, kGridLight + 4u);
  const std::uint32_t across = half(pass, kGridLight + 6u);
  pass.charge(run(0x80046540u, 0x80046558u));
  if ((along & kMapMask) != 0u) {
    return;
  }
  pass.charge(run(0x8004655Cu, 0x80046560u));
  if ((across & kMapMask) != 0u) {
    return;
  }
  pass.charge(run(0x80046564u, 0x800465C4u));
  const std::uint32_t cell = along + (across << 4);
  const std::uint32_t colour = (half(pass, kGridColour + 4u) >> 3) | ((half(pass, kGridColour + 6u) >> 3) << 8) |
                               ((half(pass, kGridColour + 0x12u) >> 3) << 16);
  pass.setGteData(psx::gte::kRgbc, colour);
  const std::uint32_t shade = byte(pass, cell + word(pass, kShades)) << 5;
  pass.setGteData(psx::gte::kIr1, shade);
  pass.setGteData(psx::gte::kIr2, shade);
  pass.setGteData(psx::gte::kIr3, shade);
  const std::uint32_t height = byte(pass, cell + word(pass, kHeights));
  pass.command(kDcpl);
  if (height == kNoHeight) {
    return;
  }
  pass.charge(run(0x800465C8u, 0x8004660Cu) + (textured ? run(0x80046610u, 0x8004661Cu) : 0u) +
              run(0x80046620u, 0x80046628u));
  const std::uint32_t x = sra(word(pass, kGridX + 4u), 3) & 0xFFFFu;
  const std::uint32_t y = sra(word(pass, kGridY + 4u) + (height << 3), 4) << 17;
  pass.setGteData(psx::gte::kVxy0, x | y);
  pass.setGteData(psx::gte::kVz0, sra(word(pass, kGridZ + 4u), 3));
  pass.store(point, pass.gteData(psx::gte::kRgb2));
  const std::uint32_t u = half(pass, kGridTexture + 4u);
  const std::uint32_t v = half(pass, kGridTexture + 6u);
  pass.command(kRtps);
  if (textured) {
    pass.storeHalf(point + 0xAu, (u >> 3) | ((v << 5) & 0xFF00u));
  }
  pass.storeScreen(point + 4u, 2);
  pass.storeHalf(point + 8u, pass.gteData(psx::gte::kSz3));
}

// Two packed lanes stepped together, the low lane's carry dropped.
void stepPair(Pass &pass, std::uint32_t offset, std::uint32_t step) {
  setWord(pass, offset, (word(pass, offset) + word(pass, step)) & kLaneCarry);
}

// 0x800461E0: the triangle in the GTE with `colours` divided into the lit grid and drawn as strips, then whole
// 40 slots farther back.
void lightWalk(Pass &pass, const std::array<std::uint32_t, 3> &colours, std::uint32_t returnAddress) {
  pass.charge(run(0x800461E0u, 0x800463E0u));
  setWord(pass, kLightReturn, returnAddress);
  for (std::uint32_t i = 0; i < 3; ++i) {
    setWord(pass, kLightColours + i * 4u, colours[i]);
    pass.storeScreen(scratch::kBase + kLightCorners + i * 8u, i);
    setWord(pass, kLightCorners + 4u + i * 8u, pass.gteData(psx::gte::kSz1 + i));
  }
  const std::uint32_t vxy0 = pass.gteData(psx::gte::kVxy0);
  const std::uint32_t vxy1 = pass.gteData(psx::gte::kVxy1);
  const std::uint32_t vxy2 = pass.gteData(psx::gte::kVxy2);
  const std::uint32_t x0 = sra(vxy0 << 16, 13);
  setWord(pass, kGridX, x0);
  setWord(pass, kGridX + 4u, x0);
  setHalf(pass, kGridX + 8u, vxy1 - vxy0);
  setHalf(pass, kGridX + 0xAu, vxy2 - vxy0);
  const std::uint32_t y0 = sra(vxy0, 16);
  setWord(pass, kGridY, y0 << 3);
  setWord(pass, kGridY + 4u, y0 << 3);
  setHalf(pass, kGridY + 8u, sra(vxy1, 16) - y0);
  setHalf(pass, kGridY + 0xAu, sra(vxy2, 16) - y0);
  const std::uint32_t z0 = pass.gteData(psx::gte::kVz0);
  const std::uint32_t z1 = pass.gteData(psx::gte::kVz1);
  const std::uint32_t z2 = pass.gteData(psx::gte::kVz2);
  setWord(pass, kGridZ, z0 << 3);
  setWord(pass, kGridZ + 4u, z0 << 3);
  setHalf(pass, kGridZ + 8u, z1 - z0);
  setHalf(pass, kGridZ + 0xAu, z2 - z0);
  // Red and green interleaved in one pair of words (red's steps losing bit 15), blue alone.
  for (std::uint32_t lane = 0; lane < 2; ++lane) {
    const std::uint32_t shift = lane * 8u;
    const std::uint32_t mask = lane == 0u ? kDeltaMask : 0xFFFFu;
    const std::uint32_t first = (colours[0] >> shift) & 0xFFu;
    startLane(pass,
              kGridColour + lane * 2u,
              first << 3,
              (((colours[1] >> shift) & 0xFFu) - first) & mask,
              (((colours[2] >> shift) & 0xFFu) - first) & mask);
  }
  const std::uint32_t blue = colours[0] >> 16;
  setHalf(pass, kGridColour + 0x10u, blue << 3);
  setHalf(pass, kGridColour + 0x12u, blue << 3);
  setHalf(pass, kGridColour + 0x14u, (colours[1] >> 16) - blue);
  setHalf(pass, kGridColour + 0x16u, (colours[2] >> 16) - blue);
  // The light's cell along z and x, the steps an eighth of each edge's.
  const std::uint32_t lightZ = word(pass, scratch::kLightZ);
  const std::uint32_t z = sra(lightZ - z0, 6);
  startLane(pass,
            kGridLight,
            z & kDeltaMask,
            sra(sra(lightZ - z1, 6) - z, 3) & kDeltaMask,
            sra(sra(lightZ - z2, 6) - z, 3) & kDeltaMask);
  const std::uint32_t lightX = word(pass, scratch::kLightX) << 16;
  const std::uint32_t x = sra(lightX - (vxy0 << 16), 22);
  startLane(
      pass, kGridLight + 2u, x, sra(sra(lightX - (vxy1 << 16), 22) - x, 3), sra(sra(lightX - (vxy2 << 16), 22) - x, 3));
  const bool textured = (pass.material << 20) != 0u;
  if (textured) {
    walkTexture(pass);
  }

  pass.charge(run(0x80046524u, 0x80046538u));
  std::uint32_t point = word(pass, kLightGrid);
  for (std::uint32_t row = 0; row <= kGridRows; ++row) {
    pass.charge(1);
    for (std::uint32_t column = row; column <= kGridRows; ++column) {
      litPoint(pass, point, textured);
      pass.charge(run(0x8004662Cu, 0x800466B0u));
      stepPair(pass, kGridLight + 4u, kGridLight + 8u);
      stepPair(pass, kGridTexture + 4u, kGridTexture + 8u);
      stepPair(pass, kGridColour + 4u, kGridColour + 8u);
      setHalf(pass, kGridColour + 0x12u, half(pass, kGridColour + 0x12u) + half(pass, kGridColour + 0x14u));
      for (const std::uint32_t lane : {kGridX, kGridY, kGridZ}) {
        setWord(pass, lane + 4u, word(pass, lane + 4u) + half(pass, lane + 8u));
      }
      point += kGridPointBytes;
    }
    pass.charge(run(0x800466B4u, 0x80046750u));
    stepPair(pass, kGridLight, kGridLight + 0xCu);
    stepPair(pass, kGridTexture, kGridTexture + 0xCu);
    setWord(pass, kGridLight + 4u, word(pass, kGridLight));
    setWord(pass, kGridTexture + 4u, word(pass, kGridTexture));
    stepPair(pass, kGridColour, kGridColour + 0xCu);
    setHalf(pass, kGridColour + 0x10u, half(pass, kGridColour + 0x10u) + half(pass, kGridColour + 0x16u));
    setWord(pass, kGridColour + 4u, word(pass, kGridColour));
    setHalf(pass, kGridColour + 0x12u, half(pass, kGridColour + 0x10u));
    for (const std::uint32_t lane : {kGridX, kGridY, kGridZ}) {
      setWord(pass, lane, word(pass, lane) + half(pass, lane + 0xAu));
      setWord(pass, lane + 4u, word(pass, lane));
    }
  }

  // The strips: each new point a triangle with the two before it.
  pass.charge(run(0x80046754u, 0x80046758u));
  const auto gridPoint = [&](std::uint32_t index) {
    const std::uint32_t address = word(pass, kLightGrid) + index * kGridPointBytes;
    return LitPoint{.colour = pass.word(address),
                    .screen = address + 4u,
                    .depth = static_cast<std::uint32_t>(pass.signedHalf(address + 8u))};
  };
  const auto pointUv = [&](std::uint32_t index) {
    return static_cast<std::uint32_t>(pass.signedHalf(word(pass, kLightGrid) + index * kGridPointBytes + 0xAu));
  };
  const auto stripByte = [&](std::uint32_t address) {
    return static_cast<std::uint32_t>(pass.core().mem_r8s(address));
  };
  std::uint32_t strip = kStrips;
  std::uint32_t entry = stripByte(strip++);
  pass.charge(run(0x8004675Cu, 0x80046764u));
  while (true) {
    pass.charge(run(0x80046768u, 0x8004676Cu));
    if (entry == 0xFFFFFFFFu) {
      break;
    }
    pass.charge(run(0x80046770u, 0x800467C4u));
    std::array<LitPoint, 3> points{};
    points[0] = gridPoint(entry & 0x7Fu);
    setHalf(pass, scratch::kLightTexture, pointUv(entry & 0x7Fu));
    entry = stripByte(strip);
    points[1] = gridPoint(entry);
    setHalf(pass, scratch::kLightTexture + 4u, pointUv(entry));
    entry = stripByte(strip + 1u);
    strip += 2u;
    while (true) {
      pass.charge(run(0x800467C8u, 0x800467F4u));
      points[2] = gridPoint(entry);
      setHalf(pass, scratch::kLightTexture + 8u, pointUv(entry));
      emitLit(pass, points, textured, true, 0);
      pass.charge(run(0x800467F8u, 0x80046804u));
      entry = stripByte(strip++);
      if (negative(entry)) {
        break;
      }
      pass.charge(run(0x80046808u, 0x80046830u));
      points[0] = points[1];
      points[1] = points[2];
      setHalf(pass, scratch::kLightTexture, half(pass, scratch::kLightTexture + 4u));
      setHalf(pass, scratch::kLightTexture + 4u, half(pass, scratch::kLightTexture + 8u));
    }
  }

  pass.charge(run(0x80046834u, 0x80046870u));
  std::array<LitPoint, 3> whole{};
  for (std::uint32_t i = 0; i < 3; ++i) {
    whole[i] = {.colour = word(pass, kLightColours + i * 4u), .screen = scratch::kBase + kLightCorners + i * 8u};
    setWord(pass, scratch::kLightTexture + i * 4u, word(pass, kSavedTexture + i * 4u));
  }
  emitLit(pass, whole, textured, false, word(pass, kLightSlot) - kFarther);
  pass.charge(run(0x80046874u, 0x80046880u));
}

// The projector's start shared with the default one: RTPT, the first colour and the next entry decoded.
void projectStart(Pass &pass) {
  pass.charge(run(0x80045E04u, 0x80045E20u));
  pass.command(kRtpt);
  pass.colour[0] = pass.vertexColour(0);
  pass.decodeNext();
}

} // namespace

void projectLightTriangle(Pass &pass) {
  pass.charge(run(0x80045D88u, 0x80045D98u));
  if (!markedForLight(pass, 3) || !nearLight(pass)) {
    projectTriangle(pass);
    return;
  }
  projectStart(pass);
  pass.charge(run(0x80045E78u, 0x80045EC0u));
  const std::uint32_t screens[3] = {pass.screen(0), pass.screen(1), pass.screen(2)};
  const Cull cull = pass.offScreen(screens, 3);
  if (cull == Cull::Rows) {
    return;
  }
  if (cull == Cull::Columns) {
    return;
  }
  pass.colour[1] = pass.vertexColour(1);
  pass.colour[2] = pass.vertexColour(2);
  pass.charge(run(0x80045ECCu, 0x80045F18u) + pass.depthSlot() + run(0x80045F20u, 0x80045F2Cu));
  setWord(pass, kLightDepth, pass.depth);
  setWord(pass, kLightSlot, pass.slot);
  lightWalk(pass, {pass.colour[0], pass.colour[1], pass.colour[2]}, 0x80045F30u);
  pass.charge(run(0x80045F30u, 0x80045F3Cu));
  pass.ending = Ending::Skipped;
}

void projectLightQuad(Pass &pass) {
  pass.charge(run(0x80045F40u, 0x80045F54u));
  if (!markedForLight(pass, 4) || !nearLight(pass)) {
    projectQuad(pass);
    return;
  }
  projectStart(pass);
  pass.storeScreen(scratch::kBase + scratch::kQuadFirstXy, 0);
  setWord(pass, scratch::kQuadFirstZ, pass.gteData(psx::gte::kSz1));
  setWord(pass, scratch::kQuadFirstVxy, pass.gteData(psx::gte::kVxy0));
  setWord(pass, scratch::kQuadFirstVz, pass.gteData(psx::gte::kVz0));
  pass.setGteData(psx::gte::kVxy0, pass.lastXy);
  pass.setGteData(psx::gte::kVz0, pass.lastZ);
  setWord(pass, kLastXy, pass.lastXy);
  setWord(pass, kLastZ, pass.lastZ);
  std::uint32_t screens[4] = {pass.screen(0), pass.screen(1), pass.screen(2), 0};
  pass.command(kRtps);
  pass.colour[1] = pass.vertexColour(1);
  screens[3] = pass.screen(2);
  pass.charge(run(0x80046034u, 0x800460CCu));
  const Cull cull = pass.offScreen(screens, 4);
  if (cull == Cull::Rows) {
    return;
  }
  if (cull == Cull::Columns) {
    return;
  }
  pass.colour[2] = pass.vertexColour(2);
  pass.colour[3] = pass.vertexColour(3);
  pass.charge(run(0x800460D8u, 0x80046134u) + pass.depthSlot() + run(0x8004613Cu, 0x80046190u));
  setWord(pass, kLightSlot, pass.slot);
  setWord(pass, kLightDepth, pass.depth);
  // The second triangle first (corners 1, 2, 3, its texture turned), from vectors kept in the scratchpad.
  setWord(pass, scratch::kRotated, kVertexMask);
  setWord(pass, kMaterial, pass.material);
  for (std::uint32_t i = 0; i < 2; ++i) {
    const std::uint32_t vector = psx::gte::kVxy1 + i * 2u;
    setWord(pass, kLightVectors + i * 8u, pass.gteData(vector));
    setWord(pass, kLightVectors + 4u + i * 8u, pass.gteData(vector + 1u));
    pass.setGteData(vector - 2u, word(pass, kLightVectors + i * 8u));
    pass.setGteData(vector - 1u, word(pass, kLightVectors + 4u + i * 8u));
  }
  pass.setGteData(psx::gte::kVxy2, word(pass, kLastXy));
  pass.setGteData(psx::gte::kVz2, word(pass, kLastZ));
  setWord(pass, kFirstColour, pass.colour[0]);
  lightWalk(pass, {pass.colour[1], pass.colour[2], pass.colour[3]}, 0x80046194u);

  pass.charge(run(0x80046194u, 0x800461CCu));
  pass.material = word(pass, kMaterial);
  const std::uint32_t second = word(pass, kLightColours);
  const std::uint32_t third = word(pass, kLightColours + 4u);
  pass.setGteData(psx::gte::kSxy1, word(pass, kLightCorners));
  pass.setGteData(psx::gte::kSxy2, word(pass, kLightCorners + 8u));
  pass.setGteData(psx::gte::kSxy0, word(pass, scratch::kQuadFirstXy));
  for (std::uint32_t i = 0; i < 2; ++i) {
    pass.setGteData(psx::gte::kVxy1 + i * 2u, word(pass, kLightVectors + i * 8u));
    pass.setGteData(psx::gte::kVz1 + i * 2u, word(pass, kLightVectors + 4u + i * 8u));
  }
  pass.setGteData(psx::gte::kVxy0, word(pass, scratch::kQuadFirstVxy));
  pass.setGteData(psx::gte::kVz0, word(pass, scratch::kQuadFirstVz));
  setWord(pass, scratch::kRotated, 0);
  lightWalk(pass, {word(pass, kFirstColour), second, third}, 0x800461D0u);
  pass.charge(run(0x800461D0u, 0x800461DCu));
  pass.ending = Ending::Skipped;
}

} // namespace crash2::world
