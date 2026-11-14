// The scene modes that recolour corners or move vertices before the default steps (crash2-decomp
// docs/notes/world-renderer.md, "Modes"). Instruction charges follow the retail paths; where the retail code skips a
// CTC2 that would rewrite the same far colour, the write here is repeated, which leaves the GTE the same.
#include "crash2/render/world_steps.h"

#include "gte_registers.h"

namespace crash2::world {
namespace {

using psx::gte::kDcpl;
using psx::gte::kDpcs;

inline constexpr std::uint32_t kShiftMask = 31u;

// RGBC loaded, DPCS run, RGB2 returned.
std::uint32_t depthCue(Pass &pass, std::uint32_t colour) {
  pass.setGteData(psx::gte::kRgbc, colour);
  pass.command(kDpcs);
  return pass.gteData(psx::gte::kRgb2);
}

// ---- Fog: each corner toward 0xFF0 (bit 18) or 0x200 by the scene's depth.

inline constexpr std::uint32_t kFogBright = 0xFF0u;
inline constexpr std::uint32_t kFogDark = 0x200u;

void fogCorners(Pass &pass, int corners) {
  pass.charge(run(0x800459A8u, 0x800459B8u));
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kShade));
  bool previous = false;
  for (int corner = 0; corner < corners; ++corner) {
    const bool bright = (pass.words[corner] & kBit18) != 0u;
    if (corner == 0) {
      pass.charge(bright ? run(0x80045A34u, 0x80045A58u) : run(0x800459BCu, 0x800459DCu));
    } else {
      pass.charge(bright == previous ? run(0x80045A68u, 0x80045A84u) : run(0x800459E0u, 0x80045A08u));
    }
    const std::uint32_t far = bright ? kFogBright : kFogDark;
    pass.setFarColour(far, far, far);
    pass.colour[corner] = depthCue(pass, pass.colour[corner]);
    previous = bright;
  }
}

// ---- Depth fog: corners past the start depth fogged by how far (shifted into IR0).

bool depthFogCorner(Pass &pass, int corner, const std::array<std::uint32_t, 4> &depths) {
  const std::uint32_t past = depths[corner] - pass.scratchWord(scratch::kFogStart);
  if (negative(past)) {
    return false;
  }
  pass.setGteData(psx::gte::kRgbc, pass.colour[corner]);
  pass.setGteData(psx::gte::kIr0, past << (pass.scratchWord(scratch::kFogShift) & kShiftMask));
  pass.command(kDpcs);
  pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
  return true;
}

// ---- Tint and pulse: bit 19's corners toward the far colour by IR0.

void cueMarked(Pass &pass, int corners) {
  for (int corner = 0; corner < corners; ++corner) {
    pass.setGteData(psx::gte::kRgbc, pass.colour[corner]);
    if ((pass.words[corner] & kBit19) != 0u) {
      pass.command(kDpcs);
      pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
    }
  }
}

std::uint32_t markedCount(const Pass &pass, int corners) {
  std::uint32_t count = 0;
  for (int corner = 0; corner < corners; ++corner) {
    if ((pass.words[corner] & kBit19) != 0u) {
      ++count;
    }
  }
  return count;
}

// ---- Dimming by depth (pulse's bit 18 polygons, waves' all-bit-19 ones): black nearer than 0x258, toward black
// out to `span` past it; whether any colour is left. The quad's first depth is the one its projector kept.

inline constexpr std::uint32_t kDimNear = 0x258u;
inline constexpr std::uint32_t kDimShift = 5u;

template <std::uint32_t Span> bool dimCorners(Pass &pass, int corners) {
  std::array<std::uint32_t, 4> depths{};
  if (corners == 4) {
    depths = {pass.scratchWord(scratch::kQuadFirstZ),
              pass.gteData(psx::gte::kSz1),
              pass.gteData(psx::gte::kSz2),
              pass.gteData(psx::gte::kSz3)};
    pass.charge(run(0x80043EC0u, 0x80043ED4u) + run(0x80043EF8u, 0x80043F08u) + run(0x80043F2Cu, 0x80043F3Cu) +
                run(0x80043F60u, 0x80043F6Cu) + run(0x80043F90u, 0x80043FA0u));
  } else {
    depths = {pass.gteData(psx::gte::kSz1), pass.gteData(psx::gte::kSz2), pass.gteData(psx::gte::kSz3), 0};
    pass.charge(run(0x80043DA0u, 0x80043DB4u) + run(0x80043DD8u, 0x80043DE8u) + run(0x80043E0Cu, 0x80043E18u) +
                run(0x80043E3Cu, 0x80043E48u));
  }
  std::uint32_t left = 0;
  for (int corner = 0; corner < corners; ++corner) {
    const std::uint32_t depth = depths[corner] - kDimNear;
    const std::uint32_t past = Span - depth;
    if (!negative(past)) {
      pass.charge(run(0x80043DB8u, 0x80043DC0u));
      pass.setGteData(psx::gte::kRgbc, pass.colour[corner]);
      if (static_cast<std::int32_t>(depth) <= 0) {
        pass.colour[corner] = 0;
      } else {
        pass.charge(run(0x80043DC4u, 0x80043DD4u));
        pass.setGteData(psx::gte::kIr0, past << kDimShift);
        pass.command(kDpcs);
        pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
      }
    }
    left |= pass.colour[corner];
  }
  if (left == 0u) {
    pass.charge(run(0x80043E4Cu, 0x80043E50u));
  }
  return left != 0u;
}

// ---- Lamps: a lamp's reach left at a vertex, less how far it is along z, y and x (`flat` off before x).

struct Reach {
  std::uint32_t left = 0;
  // The axes taken before the reach ran out (or all three), and the magnitudes negated among them.
  std::uint32_t axes = 0;
  std::uint32_t negations = 0;

  bool out() const {
    return negative(left);
  }
  // The retail test's instructions after its `head`: three per axis's test and two more per later axis's load.
  std::uint32_t instructions(std::uint32_t head) const {
    return head + negations + 3u + (axes - 1u) * 5u;
  }
};

std::uint32_t magnitude(std::uint32_t value, Reach &reach) {
  if (negative(value)) {
    ++reach.negations;
    return 0u - value;
  }
  return value;
}

// A vertex's vector, or the corner's taken for it.
struct Point {
  std::uint32_t xy = 0;
  std::uint32_t z = 0;
};

// A lamp's position and how far it reaches (`flat` taken off before the last axis).
struct Lamp {
  std::uint32_t xy = 0;
  std::uint32_t z = 0;
  std::uint32_t start = 0;
  std::uint32_t flat = 0;
};

Reach reach(const Point &vertex, const Lamp &lamp) {
  Reach result;
  const std::uint32_t offset = vertex.xy - lamp.xy;
  result.axes = 1;
  result.left = lamp.start - magnitude(vertex.z - lamp.z, result);
  if (result.out()) {
    return result;
  }
  result.axes = 2;
  result.left -= magnitude(sra(offset, 16), result);
  if (result.out()) {
    return result;
  }
  result.axes = 3;
  result.left = result.left - lamp.flat - magnitude(sra(offset << 16, 16), result);
  return result;
}

inline constexpr std::uint32_t kLampReach = 0x1320u;
inline constexpr std::uint32_t kLampFlat = 0x320u;
inline constexpr std::uint32_t kLampFull = 0x1000u;

// 0x80042D10: a corner lit by the two lamps from its vertex's vector; the result in RGB2 (left as it was for a vertex
// with bit 18).
void lampCorner(Pass &pass, const Point &at, std::uint32_t vertex) {
  if ((vertex & kBit19) != 0u) {
    pass.charge(run(0x80042D10u, 0x80042D18u) + run(0x80042E50u, 0x80042E80u));
    pass.setFarColour(pass.scratchWord(scratch::kTintFar),
                      pass.scratchWord(scratch::kTintFar + 4u),
                      pass.scratchWord(scratch::kTintFar + 8u));
    pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kTint));
    pass.setGteData(psx::gte::kRgbc, pass.scratchWord(scratch::kCameraZ));
    pass.command(kDpcs);
    return;
  }
  if ((vertex & kBit18) != 0u) {
    pass.charge(run(0x80042D10u, 0x80042D20u) + run(0x80042E48u, 0x80042E4Cu));
    return;
  }
  pass.charge(run(0x80042D10u, 0x80042D20u));
  const Reach first = reach(at,
                            {.xy = pass.scratchWord(scratch::kLamp1Xy),
                             .z = pass.scratchWord(scratch::kLamp1Z),
                             .start = kLampReach,
                             .flat = kLampFlat});
  pass.charge(first.instructions(run(0x80042D24u, 0x80042D2Cu)) + (first.out() ? 1u : 0u));
  const Reach second = reach(at,
                             {.xy = pass.scratchWord(scratch::kLamp2Xy),
                              .z = pass.scratchWord(scratch::kLamp2Z),
                              .start = kLampReach,
                              .flat = kLampFlat});
  pass.charge(second.instructions(run(0x80042D74u, 0x80042D80u)) + (second.out() ? 1u : 0u));
  const std::uint32_t light = (first.out() ? 0u : first.left) + (second.out() ? 0u : second.left);
  pass.charge(run(0x80042DC8u, 0x80042DD0u));
  if (light == 0u) {
    pass.charge(run(0x80042E28u, 0x80042E30u));
    pass.setGteData(psx::gte::kIr1, pass.scratchWord(scratch::kLampFar));
    pass.setGteData(psx::gte::kIr2, pass.scratchWord(scratch::kLampFar + 4u));
    pass.setGteData(psx::gte::kIr3, pass.scratchWord(scratch::kLampFar + 8u));
  } else {
    const std::uint32_t between = kLampFull - light;
    pass.charge(run(0x80042DD4u, 0x80042DDCu));
    if (static_cast<std::int32_t>(between) <= 0) {
      pass.charge(run(0x80042E14u, 0x80042E24u));
      pass.setGteData(psx::gte::kIr1, pass.scratchWord(scratch::kLampFull));
      pass.setGteData(psx::gte::kIr2, pass.scratchWord(scratch::kLampFull + 4u));
      pass.setGteData(psx::gte::kIr3, pass.scratchWord(scratch::kLampFull + 8u));
    } else {
      pass.charge(run(0x80042DE0u, 0x80042E10u));
      pass.setFarColour(pass.scratchWord(scratch::kLampFar),
                        pass.scratchWord(scratch::kLampFar + 4u),
                        pass.scratchWord(scratch::kLampFar + 8u));
      pass.setGteData(psx::gte::kRgbc, pass.scratchWord(scratch::kShade));
      pass.setGteData(psx::gte::kIr0, between);
      pass.command(kDpcs);
    }
  }
  pass.charge(run(0x80042E34u, 0x80042E4Cu));
  pass.setGteData(psx::gte::kIr0, 0);
  pass.setGteData(psx::gte::kRgbc, pass.scratchWord(scratch::kCameraZ));
  pass.command(kDcpl);
}

// The lit modes keep the vertices' words, the next entry's kind and the depth in the scratchpad.
void keepLit(Pass &pass, int corners) {
  for (int corner = 0; corner < corners; ++corner) {
    pass.storeScratch(scratch::kLitWords + static_cast<std::uint32_t>(corner) * 4u, pass.words[corner]);
  }
  pass.storeScratch(scratch::kUpcoming, pass.upcoming.kind);
  pass.storeScratch(scratch::kLitDepth, pass.depth);
}

// A corner's vector: the quad's first as its projector kept it, its others in GTE vectors 1, 2 and 0.
std::array<Point, 4> litVectors(Pass &pass, int corners) {
  static constexpr std::array<std::uint32_t, 4> kTriangleVectors{psx::gte::kVxy0, psx::gte::kVxy1, psx::gte::kVxy2, 0};
  static constexpr std::array<std::uint32_t, 4> kQuadVectors{0, psx::gte::kVxy1, psx::gte::kVxy2, psx::gte::kVxy0};
  std::array<Point, 4> points{};
  for (int corner = 0; corner < corners; ++corner) {
    if (corners == 4 && corner == 0) {
      points[0] = {.xy = pass.scratchWord(scratch::kQuadFirstVxy), .z = pass.scratchWord(scratch::kQuadFirstVz)};
      continue;
    }
    const std::uint32_t vector = corners == 4 ? kQuadVectors[corner] : kTriangleVectors[corner];
    points[corner] = {.xy = pass.gteData(vector), .z = pass.gteData(vector + 1u)};
  }
  return points;
}

void lampColours(Pass &pass, int corners) {
  keepLit(pass, corners);
  const std::array<Point, 4> points = litVectors(pass, corners);
  for (int corner = 0; corner < corners; ++corner) {
    pass.storeScratch(scratch::kCameraZ, pass.colour[corner]);
    lampCorner(pass, points[corner], pass.scratchWord(scratch::kLitWords + static_cast<std::uint32_t>(corner) * 4u));
    pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
  }
}

// ---- Glow: what is left of a lamp's reach lights a corner; near neither lamp, a black flat polygon.

inline constexpr std::uint32_t kGlowFirstReach = 0x15E0u;
inline constexpr std::uint32_t kGlowReach = 0x12C0u;
inline constexpr std::uint32_t kGlowTinted = 0x199u + 0x332u;

enum class GlowLamps : std::uint8_t {
  None,
  One,
  Both,
};

// 0x80044C88: a corner's colour from its light (into RGB2).
// A lit corner: its vertex's word and the light left at it.
struct Lit {
  std::uint32_t vertex = 0;
  std::uint32_t light = 0;
};

void glowCorner(Pass &pass, Lit lit) {
  const std::uint32_t vertex = lit.vertex;
  std::uint32_t light = lit.light;
  pass.charge(run(0x80044C88u, 0x80044C90u));
  const std::uint32_t colour = pass.scratchWord(scratch::kCameraZ);
  if ((vertex & kBit19) != 0u) {
    pass.charge(run(0x80044CA8u, 0x80044CB4u));
    light += kGlowTinted;
  } else {
    pass.charge(run(0x80044C94u, 0x80044C98u));
    if ((vertex & kBit18) != 0u) {
      pass.charge(run(0x80044C9Cu, 0x80044CA4u));
      pass.setGteData(psx::gte::kRgb2, colour);
      return;
    }
  }
  pass.charge(run(0x80044CB8u, 0x80044CBCu));
  if (light == 0u) {
    pass.charge(run(0x80044CECu, 0x80044CF4u));
    pass.setGteData(psx::gte::kRgb2, 0);
    return;
  }
  const std::uint32_t between = kLampFull - light;
  pass.charge(run(0x80044CC0u, 0x80044CD0u) + run(0x80044CD4u, 0x80044CDCu));
  pass.setGteData(psx::gte::kIr0, between);
  pass.setGteData(psx::gte::kRgbc, colour);
  if (static_cast<std::int32_t>(between) <= 0) {
    pass.setGteData(psx::gte::kRgb2, colour);
  } else {
    pass.command(kDpcs);
  }
}

std::uint32_t atLeastZero(std::uint32_t value) {
  return negative(value) ? 0u : value;
}

// Returns false when the polygon was drawn black and flat (linked by the draw tail).
bool glowColours(Pass &pass, int corners) {
  keepLit(pass, corners);
  const std::array<Point, 4> points = litVectors(pass, corners);
  const Lamp lamp1{.xy = pass.scratchWord(scratch::kLamp1Xy), .z = pass.scratchWord(scratch::kLamp1Z)};
  const Lamp lamp2{.xy = pass.scratchWord(scratch::kLamp2Xy), .z = pass.scratchWord(scratch::kLamp2Z)};
  const auto within = [](Lamp lamp, std::uint32_t start) {
    lamp.start = start;
    return lamp;
  };
  pass.storeScratch(scratch::kCameraZ, pass.colour[0]);
  // The lamp a one-lamp polygon's later corners take.
  Lamp only = lamp1;
  GlowLamps lamps = GlowLamps::None;
  std::uint32_t light = 0;
  const Reach first = reach(points[0], within(lamp1, kGlowFirstReach));
  pass.charge(first.instructions(run(0x80044BB4u, 0x80044BD0u)));
  const Reach second = reach(points[0], within(lamp2, kGlowFirstReach));
  pass.charge(second.instructions(run(0x80044C14u, 0x80044C20u)));
  if (!first.out() && !second.out()) {
    lamps = GlowLamps::Both;
    const std::uint32_t near = first.left - kLampFlat;
    const std::uint32_t far = second.left - kLampFlat;
    pass.charge(run(0x80044C64u, 0x80044C74u) + (negative(near) ? 1u : 0u) + run(0x80044C7Cu, 0x80044C80u) +
                (negative(far) ? 1u : 0u));
    light = negative(far) ? atLeastZero(near) : atLeastZero(near) + far;
  } else if (!first.out()) {
    lamps = GlowLamps::One;
    light = first.left - kLampFlat;
    pass.charge(run(0x80044E2Cu, 0x80044E3Cu) + (negative(light) ? 2u : 0u));
    light = atLeastZero(light);
  } else if (!second.out()) {
    lamps = GlowLamps::One;
    only = lamp2;
    light = second.left - kLampFlat;
    pass.charge(run(0x80044D48u, 0x80044D5Cu) + (negative(light) ? 2u : 0u));
    light = atLeastZero(light);
  } else {
    std::uint32_t special = 0;
    for (int corner = 0; corner < corners; ++corner) {
      special |= pass.scratchWord(scratch::kLitWords + static_cast<std::uint32_t>(corner) * 4u);
    }
    pass.charge(run(0x80044D68u, 0x80044D6Cu) +
                (corners == 4 ? run(0x80044DC8u, 0x80044DF8u) : run(0x80044D70u, 0x80044D94u)));
    if (((special << 12) >> 30) == 0u) {
      pass.charge(corners == 4 ? run(0x80044DFCu, 0x80044E28u) : run(0x80044D98u, 0x80044DC4u));
      if (corners == 4) {
        drawFlatQuad(pass);
      } else {
        drawFlatTriangle(pass);
      }
      return false;
    }
  }
  // The first lamp's test when it ran out leads the second's at 0x80044CF8 instead of 0x80044C14; same length.
  for (int corner = 0; corner < corners; ++corner) {
    if (corner > 0) {
      pass.colour[corner - 1] = pass.gteData(psx::gte::kRgb2);
      pass.storeScratch(scratch::kCameraZ, pass.colour[corner]);
      switch (lamps) {
      case GlowLamps::None:
        pass.charge(run(0x80044AA0u, 0x80044AA4u));
        light = 0;
        break;
      case GlowLamps::One: {
        const Reach left = reach(points[corner], within(only, kGlowReach));
        pass.charge(left.instructions(run(0x80044AA8u, 0x80044AB4u)) + (left.out() ? 2u : 0u));
        light = atLeastZero(left.left);
        break;
      }
      case GlowLamps::Both: {
        const Reach near = reach(points[corner], within(lamp1, kGlowReach));
        const Reach far = reach(points[corner], within(lamp2, kGlowReach));
        pass.charge(near.instructions(run(0x80044B00u, 0x80044B0Cu)) + (near.out() ? 1u : 0u) +
                    far.instructions(run(0x80044B54u, 0x80044B60u)) + 2u);
        light = far.out() ? atLeastZero(near.left) : atLeastZero(near.left) + far.left;
        break;
      }
      }
    }
    glowCorner(
        pass,
        {.vertex = pass.scratchWord(scratch::kLitWords + static_cast<std::uint32_t>(corner) * 4u), .light = light});
  }
  pass.colour[corners - 1] = pass.gteData(psx::gte::kRgb2);
  return true;
}

// ---- Waves: bit 18's vertices moved along y by the scratchpad's wave table.

std::uint32_t waved(Pass &pass, std::uint32_t xy, std::uint32_t phase) {
  const std::uint32_t index = (((xy >> 16) + phase + xy) >> 4) & 0xFu;
  return xy + (static_cast<std::uint32_t>(pass.core().mem_r8(scratch::kBase + scratch::kWaves + index)) << 16);
}

void waveVector(Pass &pass, std::uint32_t vector, std::uint32_t phase) {
  pass.setGteData(vector, waved(pass, pass.gteData(vector), phase));
}

// ---- Sway: bit 18's vertices moved by a sine of their position, the camera's and the clock.

inline constexpr std::uint32_t kSineTable = 0x8005BCB0u;

struct SwayState {
  std::uint32_t clock = 0;
  std::uint32_t scale = 0;
  std::uint32_t phase = 0;
};

// 0x800429D0: the pair and z moved; returns the instructions it took.
std::uint32_t sway(Pass &pass, Point &vertex, const SwayState &state) {
  Core &core = pass.core();
  std::uint32_t &xy = vertex.xy;
  std::uint32_t &z = vertex.z;
  std::uint32_t instructions = run(0x800429D0u, 0x800429E0u) + run(0x800429E8u, 0x800429F8u) + 3u;
  std::uint32_t angle = sra(z + state.phase + xy, 4);
  const bool flip = (angle & 0x20u) != 0u;
  angle += state.clock;
  if (flip) {
    angle = 0u - angle;
    ++instructions;
  }
  const bool upper = negative(angle << 27);
  const std::uint32_t step = (angle << 27) >> 20;
  const bool back = negative(angle << 26);
  const auto scaled = [&](std::uint32_t address) {
    return sra(static_cast<std::uint32_t>(core.mem_r16s(address)) << 16, state.scale);
  };
  if (!upper) {
    const std::uint32_t cosine = scaled(kSineTable - step + 0x800u);
    const std::uint32_t sine = scaled(kSineTable + step);
    if (!back) {
      instructions += run(0x80042A08u, 0x80042A2Cu);
      z += cosine;
      xy += sine;
    } else {
      instructions += run(0x80042A30u, 0x80042A58u);
      z -= cosine;
      xy -= sine;
    }
    return instructions;
  }
  const std::uint32_t along = scaled(kSineTable - step + 0x1000u);
  const std::uint32_t across = scaled(kSineTable + step - 0x800u);
  if (!back) {
    instructions += run(0x80042A68u, 0x80042A8Cu);
    xy += along;
    z -= across;
  } else {
    instructions += run(0x80042A90u, 0x80042AB4u);
    xy -= along;
    z += across;
  }
  return instructions;
}

SwayState swayState(Pass &pass) {
  pass.storeScratch(scratch::kUpcoming, pass.upcomingEntry);
  return {.clock = sra(pass.scratchWord(scratch::kClock), pass.scratchWord(scratch::kSwayShift)),
          .scale = pass.scratchWord(scratch::kSwayScale),
          .phase = pass.scratchWord(scratch::kPhase)};
}

void swayVector(Pass &pass, const SwayState &state, std::uint32_t vector) {
  Point vertex{.xy = pass.gteData(vector), .z = pass.gteData(vector + 1u)};
  pass.charge(sway(pass, vertex, state));
  pass.setGteData(vector, vertex.xy);
  pass.setGteData(vector + 1u, vertex.z);
}

void swayRecolour(Pass &pass, int corners) {
  const std::uint32_t colour = pass.scratchWord(scratch::kModeColour);
  if (colour == 0u) {
    return;
  }
  for (int corner = 0; corner < corners; ++corner) {
    if ((pass.words[corner] & kBit19) != 0u) {
      pass.colour[corner] = colour;
    }
  }
}

} // namespace

void drawFogTriangle(Pass &pass) {
  fogCorners(pass, 3);
  drawTriangle(pass);
}

void drawFogQuad(Pass &pass) {
  fogCorners(pass, 4);
  drawQuad(pass);
}

void drawDepthFogTriangle(Pass &pass) {
  pass.charge(run(0x800439D4u, 0x800439ECu) + run(0x80043A08u, 0x80043A14u) + run(0x80043A30u, 0x80043A38u));
  const std::uint32_t sz1 = pass.gteData(psx::gte::kSz1);
  const std::uint32_t sz2 = pass.gteData(psx::gte::kSz2);
  const std::uint32_t sz3 = pass.gteData(psx::gte::kSz3);
  const std::array<std::uint32_t, 4> depths{sz1, sz2, sz3, 0};
  bool fogged = false;
  for (int corner = 0; corner < 3; ++corner) {
    fogged = depthFogCorner(pass, corner, depths);
    if (fogged) {
      pass.charge(run(0x800439F0u, 0x80043A04u));
    }
  }
  if (!fogged) {
    drawTriangle(pass);
    return;
  }
  pass.charge(run(0x80043A54u, 0x80043A60u));
  if ((pass.colour[0] | pass.colour[1] | pass.colour[2]) != 0u) {
    drawTriangle(pass);
    return;
  }
  pass.charge(run(0x80043A64u, 0x80043A80u));
  drawFlatTriangle(pass);
}

void drawDepthFogQuad(Pass &pass) {
  pass.charge(run(0x80043A84u, 0x80043A9Cu) + run(0x80043AB8u, 0x80043AC4u) + run(0x80043AE0u, 0x80043AECu) +
              run(0x80043B08u, 0x80043B10u));
  const std::array<std::uint32_t, 4> depths{pass.scratchWord(scratch::kQuadFirstZ),
                                            pass.gteData(psx::gte::kSz1),
                                            pass.gteData(psx::gte::kSz2),
                                            pass.gteData(psx::gte::kSz3)};
  bool fogged = false;
  for (int corner = 0; corner < 4; ++corner) {
    fogged = depthFogCorner(pass, corner, depths);
    if (fogged) {
      pass.charge(run(0x80043AA0u, 0x80043AB4u));
    }
  }
  if (!fogged) {
    drawQuad(pass);
    return;
  }
  pass.charge(run(0x80043B2Cu, 0x80043B38u));
  if ((pass.colour[0] | pass.colour[1] | pass.colour[2] | pass.colour[3]) != 0u) {
    drawQuad(pass);
    return;
  }
  pass.charge(run(0x80043B3Cu, 0x80043B58u));
  drawFlatQuad(pass);
}

void drawTintTriangle(Pass &pass) {
  pass.charge(run(0x800449B0u, 0x800449C0u) + run(0x800449CCu, 0x800449D8u) + run(0x800449E4u, 0x800449F0u) +
              run(0x800449FCu, 0x80044A00u) + 2u * markedCount(pass, 3));
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kTint));
  cueMarked(pass, 3);
  drawTriangle(pass);
}

void drawTintQuad(Pass &pass) {
  pass.charge(run(0x80044A04u, 0x80044A14u) + run(0x80044A20u, 0x80044A2Cu) + run(0x80044A38u, 0x80044A44u) +
              run(0x80044A50u, 0x80044A5Cu) + run(0x80044A68u, 0x80044A6Cu) + 2u * markedCount(pass, 4));
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kTint));
  cueMarked(pass, 4);
  drawQuad(pass);
}

namespace {

inline constexpr std::uint32_t kPulseSpan = 0x2D8u;
inline constexpr std::uint32_t kWaveSpan = 0x3A0u;

// 0x80043E54 / 0x80043FAC: the bit 19 corners pulse.
void pulseCorners(Pass &pass, int corners) {
  const std::uint32_t marked = markedCount(pass, corners);
  const bool last = (pass.words[corners - 1] & kBit19) != 0u;
  if (corners == 4) {
    pass.charge(run(0x80043FACu, 0x80043FBCu) + 3u * 3u + 2u * (marked - (last ? 1u : 0u)) + (last ? 4u : 0u));
  } else {
    pass.charge(run(0x80043E54u, 0x80043E64u) + 3u * 2u + 2u * (marked - (last ? 1u : 0u)) + (last ? 4u : 0u));
  }
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kPulse));
  cueMarked(pass, corners);
}

void pulsePolygon(Pass &pass, int corners, Step draw) {
  std::uint32_t any = 0;
  for (int corner = 0; corner < corners; ++corner) {
    any |= pass.words[corner];
  }
  pass.charge(corners == 4 ? run(0x80043EA0u, 0x80043EB4u) : run(0x80043D84u, 0x80043D94u));
  if ((any & kBit19) != 0u) {
    pulseCorners(pass, corners);
    draw(pass);
    return;
  }
  pass.charge(2);
  if ((pass.words[0] & kBit18) == 0u) {
    draw(pass);
    return;
  }
  if (!dimCorners<kPulseSpan>(pass, corners)) {
    return;
  }
  draw(pass);
}

void wavePolygon(Pass &pass, int corners, Step draw) {
  std::uint32_t all = ~0u;
  for (int corner = 0; corner < corners; ++corner) {
    all &= pass.words[corner];
  }
  pass.charge(corners == 4 ? run(0x80043D34u, 0x80043D48u) : run(0x80043C2Cu, 0x80043C3Cu));
  if ((all & kBit19) == 0u) {
    draw(pass);
    return;
  }
  pass.charge(2);
  if (!dimCorners<kWaveSpan>(pass, corners)) {
    return;
  }
  draw(pass);
}

} // namespace

void drawPulseTriangle(Pass &pass) {
  pulsePolygon(pass, 3, drawTriangle);
}

void drawPulseQuad(Pass &pass) {
  pulsePolygon(pass, 4, drawQuad);
}

void drawLampTriangle(Pass &pass) {
  pass.charge(run(0x80042E84u, 0x80042EB8u) + 2u * run(0x80042EBCu, 0x80042ED0u) + run(0x80042EECu, 0x80042F04u));
  lampColours(pass, 3);
  drawTriangle(pass);
}

void drawLampQuad(Pass &pass) {
  pass.charge(run(0x80042F08u, 0x80042F40u) + 3u * run(0x80042F44u, 0x80042F58u) + run(0x80042F8Cu, 0x80042FA4u));
  lampColours(pass, 4);
  drawQuad(pass);
}

void drawGlowTriangle(Pass &pass) {
  pass.charge(run(0x80044E48u, 0x80044E70u));
  if (!glowColours(pass, 3)) {
    return;
  }
  pass.charge(2u * run(0x80044E74u, 0x80044E88u) + run(0x80044EA4u, 0x80044EBCu));
  drawTriangle(pass);
}

void drawGlowQuad(Pass &pass) {
  pass.charge(run(0x80044EC0u, 0x80044EECu));
  if (!glowColours(pass, 4)) {
    return;
  }
  pass.charge(3u * run(0x80044EF0u, 0x80044F04u) + run(0x80044F38u, 0x80044F50u));
  drawQuad(pass);
}

void projectWaveTriangle(Pass &pass) {
  const std::uint32_t phase = pass.scratchWord(scratch::kPhase);
  pass.charge(run(0x80043B74u, 0x80043B80u) + 2u * run(0x80043BB8u, 0x80043BBCu));
  if ((pass.words[0] & kBit18) != 0u) {
    pass.charge(run(0x80043B84u, 0x80043BB4u));
    waveVector(pass, psx::gte::kVxy0, phase);
  }
  if ((pass.words[1] & kBit18) != 0u) {
    pass.charge(run(0x80043BC0u, 0x80043BF0u));
    waveVector(pass, psx::gte::kVxy1, phase);
  }
  if ((pass.words[2] & kBit18) != 0u) {
    pass.charge(run(0x80043BFCu, 0x80043C28u));
    pass.lastXy = waved(pass, pass.lastXy, phase);
    pass.setGteData(psx::gte::kVxy2, pass.lastXy);
  }
  projectTriangle(pass);
}

void projectWaveQuad(Pass &pass) {
  const std::uint32_t phase = pass.scratchWord(scratch::kPhase);
  pass.charge(run(0x80043C48u, 0x80043C54u) + 3u * run(0x80043C8Cu, 0x80043C90u));
  if ((pass.words[0] & kBit18) != 0u) {
    pass.charge(run(0x80043C58u, 0x80043C88u));
    waveVector(pass, psx::gte::kVxy0, phase);
  }
  if ((pass.words[1] & kBit18) != 0u) {
    pass.charge(run(0x80043C94u, 0x80043CC4u));
    waveVector(pass, psx::gte::kVxy1, phase);
  }
  if ((pass.words[2] & kBit18) != 0u) {
    pass.charge(run(0x80043CD0u, 0x80043D00u));
    waveVector(pass, psx::gte::kVxy2, phase);
  }
  if ((pass.words[3] & kBit18) != 0u) {
    pass.charge(run(0x80043D0Cu, 0x80043D30u));
    pass.lastXy = waved(pass, pass.lastXy, phase);
  }
  projectQuad(pass);
}

void drawWaveTriangle(Pass &pass) {
  wavePolygon(pass, 3, drawTriangle);
}

void drawWaveQuad(Pass &pass) {
  wavePolygon(pass, 4, drawQuad);
}

void projectSwayTriangle(Pass &pass) {
  pass.charge(run(0x80042AB8u, 0x80042AD4u) + 2u * run(0x80042AF0u, 0x80042AF8u) + run(0x80042B34u, 0x80042B40u));
  const SwayState state = swayState(pass);
  if ((pass.words[0] & kBit18) != 0u) {
    pass.charge(run(0x80042AD8u, 0x80042AECu));
    swayVector(pass, state, psx::gte::kVxy0);
  }
  if ((pass.words[1] & kBit18) != 0u) {
    pass.charge(run(0x80042AFCu, 0x80042B10u));
    swayVector(pass, state, psx::gte::kVxy1);
  }
  if ((pass.words[2] & kBit18) != 0u) {
    // The third vertex from the pair the dispatcher loaded, its z whole.
    Point vertex{.xy = pass.lastXy, .z = pass.lastZ};
    pass.charge(run(0x80042B20u, 0x80042B30u) + sway(pass, vertex, state));
    pass.setGteData(psx::gte::kVxy2, vertex.xy);
    pass.setGteData(psx::gte::kVz2, vertex.z);
  }
  projectTriangle(pass);
}

void projectSwayQuad(Pass &pass) {
  pass.charge(run(0x80042B7Cu, 0x80042B98u) + 3u * run(0x80042BB4u, 0x80042BBCu) + run(0x80042C1Cu, 0x80042C28u));
  const SwayState state = swayState(pass);
  if ((pass.words[0] & kBit18) != 0u) {
    pass.charge(run(0x80042B9Cu, 0x80042BB0u));
    swayVector(pass, state, psx::gte::kVxy0);
  }
  if ((pass.words[1] & kBit18) != 0u) {
    pass.charge(run(0x80042BC0u, 0x80042BD4u));
    swayVector(pass, state, psx::gte::kVxy1);
  }
  if ((pass.words[2] & kBit18) != 0u) {
    pass.charge(run(0x80042BE4u, 0x80042BF8u));
    swayVector(pass, state, psx::gte::kVxy2);
  }
  if ((pass.words[3] & kBit18) != 0u) {
    Point vertex{.xy = pass.lastXy, .z = pass.lastZ};
    pass.charge(run(0x80042C08u, 0x80042C18u) + sway(pass, vertex, state));
    pass.lastXy = vertex.xy;
    pass.lastZ = vertex.z;
  }
  projectQuad(pass);
}

void drawSwayTriangle(Pass &pass) {
  pass.charge(run(0x80042B44u, 0x80042B50u));
  if (pass.scratchWord(scratch::kModeColour) != 0u) {
    const bool last = (pass.words[2] & kBit19) != 0u;
    pass.charge(3u * 2u + markedCount(pass, 2) + (last ? 2u : 0u));
  }
  swayRecolour(pass, 3);
  drawTriangle(pass);
}

void drawSwayQuad(Pass &pass) {
  pass.charge(run(0x80042C2Cu, 0x80042C38u));
  if (pass.scratchWord(scratch::kModeColour) != 0u) {
    const bool last = (pass.words[3] & kBit19) != 0u;
    pass.charge(4u * 2u + markedCount(pass, 3) + (last ? 2u : 0u));
  }
  swayRecolour(pass, 4);
  drawQuad(pass);
}

} // namespace crash2::world
