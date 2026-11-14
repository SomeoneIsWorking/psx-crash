// The scene modes both titles' world renderers have, one implementation over the title's facts (Crash 2: crash2-decomp
// docs/notes/world-renderer.md, "Modes"; Crash 3: crash3-decomp docs/notes/world-renderer.md). Instruction counts are
// written as Crash 2's address runs; Crash 3's copies of these paths run the same counts, except where a title fact
// carries them.
#include "render/world_modes.h"

#include "gte_registers.h"
#include "render/world_default.h"

namespace crash::world {

using psx::gte::kDpcs;

// ---- Corners whose vertex has `bit` toward the far colour by IR0 (Crash 2 tint and pulse bit 19, Crash 3 pulse bit
// 18).

void cueMarked(Pass &pass, int corners, VertexBit bit) {
  for (int corner = 0; corner < corners; ++corner) {
    pass.setGteData(psx::gte::kRgbc, pass.colour[corner]);
    if ((pass.words[corner] & bit.mask) != 0u) {
      pass.command(kDpcs);
      pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
    }
  }
}

std::uint32_t markedCount(const Pass &pass, int corners, VertexBit bit) {
  std::uint32_t count = 0;
  for (int corner = 0; corner < corners; ++corner) {
    if ((pass.words[corner] & bit.mask) != 0u) {
      ++count;
    }
  }
  return count;
}

// ---- Dimming by depth (pulse's polygons, Crash 2 waves' all-bit-19 ones): black nearer than the title's near depth,
// toward black out to `span` past it; whether any colour is left. The quad's first depth is the one its projector kept.

inline constexpr std::uint32_t kDimShift = 5u;

bool dimCorners(Pass &pass, int corners, DimSpan span) {
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
    const std::uint32_t depth = depths[corner] - pass.title().pulse.dimNear;
    const std::uint32_t past = span.depth - depth;
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

namespace {

std::uint32_t magnitude(std::uint32_t value, Reach &reach) {
  if (negative(value)) {
    ++reach.negations;
    return 0u - value;
  }
  return value;
}

} // namespace

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

namespace {

// ---- Glow: what is left of a lamp's reach lights a corner; near neither lamp, a black flat polygon.

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
  const GlowFacts &glow = pass.title().glow;
  light <<= glow.lightShift;
  pass.charge(glow.cornerHead);
  const std::uint32_t colour = pass.scratchWord(glow.colourWord);
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
  const GlowFacts &glow = pass.title().glow;
  keepLit(pass, corners);
  const std::array<Point, 4> points = litVectors(pass, corners);
  const Lamp lamp1{.xy = pass.scratchWord(scratch::kLamp1Xy), .z = pass.scratchWord(scratch::kLamp1Z)};
  const Lamp lamp2{.xy = pass.scratchWord(scratch::kLamp2Xy), .z = pass.scratchWord(scratch::kLamp2Z)};
  const auto within = [](Lamp lamp, std::uint32_t start) {
    lamp.start = start;
    return lamp;
  };
  pass.storeScratch(glow.colourWord, pass.colour[0]);
  // The lamp a one-lamp polygon's later corners take.
  Lamp only = lamp1;
  GlowLamps lamps = GlowLamps::None;
  std::uint32_t light = 0;
  const Reach first = reach(points[0], within(lamp1, glow.firstReach));
  pass.charge(first.instructions(run(0x80044BB4u, 0x80044BD0u)));
  const Reach second = reach(points[0], within(lamp2, glow.firstReach));
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
      pass.charge(corners == 4 ? glow.flatQuad : glow.flatTriangle);
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
      pass.storeScratch(glow.colourWord, pass.colour[corner]);
      switch (lamps) {
      case GlowLamps::None:
        pass.charge(run(0x80044AA0u, 0x80044AA4u));
        light = 0;
        break;
      case GlowLamps::One: {
        const Reach left = reach(points[corner], within(only, glow.laterReach));
        pass.charge(left.instructions(run(0x80044AA8u, 0x80044AB4u)) + (left.out() ? 2u : 0u));
        light = atLeastZero(left.left);
        break;
      }
      case GlowLamps::Both: {
        const Reach near = reach(points[corner], within(lamp1, glow.laterReach));
        const Reach far = reach(points[corner], within(lamp2, glow.laterReach));
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

} // namespace

// ---- Sway: bit 18's vertices moved by a sine of their position, the camera's and the clock.

// Crash 2 0x800429D0, Crash 3 0x80041B7C: the pair and z moved.
Swayed sway(Pass &pass, Point &vertex, const SwayState &state) {
  Core &core = pass.core();
  const SwayFacts &facts = pass.title().sway;
  std::uint32_t &xy = vertex.xy;
  std::uint32_t &z = vertex.z;
  std::uint32_t instructions = run(0x800429D0u, 0x800429E0u) + run(0x800429E8u, 0x800429F8u) + 3u;
  std::uint32_t angle = sra(z + state.phase + xy, facts.angleShift);
  const bool flip = (angle & 0x20u) != 0u;
  angle += state.clock;
  if (flip) {
    angle = 0u - angle;
    ++instructions;
  }
  const bool upper = negative(angle << 27);
  const std::uint32_t step = (angle << 27) >> 20;
  const bool back = negative(angle << 26);
  std::uint32_t sample = 0;
  const auto scaled = [&](std::uint32_t address) {
    sample = static_cast<std::uint32_t>(core.mem_r16s(address));
    return sra(sample << 16, state.scale);
  };
  if (!upper) {
    const std::uint32_t cosine = scaled(facts.sineTable - step + 0x800u);
    const std::uint32_t sine = scaled(facts.sineTable + step);
    if (!back) {
      instructions += run(0x80042A08u, 0x80042A2Cu);
      z += cosine;
      xy += sine;
    } else {
      instructions += run(0x80042A30u, 0x80042A58u);
      z -= cosine;
      xy -= sine;
    }
    return {.instructions = instructions, .sample = sample};
  }
  const std::uint32_t across = scaled(facts.sineTable + step - 0x800u);
  const std::uint32_t along = scaled(facts.sineTable - step + 0x1000u);
  if (!back) {
    instructions += run(0x80042A68u, 0x80042A8Cu);
    xy += along;
    z -= across;
  } else {
    instructions += run(0x80042A90u, 0x80042AB4u);
    xy -= along;
    z += across;
  }
  return {.instructions = instructions, .sample = sample};
}

SwayState swayState(Pass &pass) {
  pass.storeScratch(scratch::kUpcoming, pass.upcomingEntry);
  return {.clock = sra(pass.scratchWord(scratch::kClock), pass.scratchWord(scratch::kSwayShift)),
          .scale = pass.scratchWord(scratch::kSwayScale),
          .phase = pass.scratchWord(scratch::kPhase)};
}

namespace {

void swayVector(Pass &pass, const SwayState &state, std::uint32_t vector) {
  Point vertex{.xy = pass.gteData(vector), .z = pass.gteData(vector + 1u)};
  pass.charge(sway(pass, vertex, state).instructions);
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

// The marked corners pulse (Crash 2 0x80043E54 / 0x80043FAC).
void pulseCorners(Pass &pass, int corners) {
  const std::uint32_t bit = pass.title().pulse.pulseBit;
  const std::uint32_t marked = markedCount(pass, corners, VertexBit{bit});
  const bool last = (pass.words[corners - 1] & bit) != 0u;
  if (corners == 4) {
    pass.charge(run(0x80043FACu, 0x80043FBCu) + 3u * 3u + 2u * (marked - (last ? 1u : 0u)) + (last ? 4u : 0u));
  } else {
    pass.charge(run(0x80043E54u, 0x80043E64u) + 3u * 2u + 2u * (marked - (last ? 1u : 0u)) + (last ? 4u : 0u));
  }
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kPulse));
  cueMarked(pass, corners, VertexBit{bit});
}

void pulsePolygon(Pass &pass, int corners, Step draw) {
  const PulseFacts &facts = pass.title().pulse;
  std::uint32_t any = 0;
  for (int corner = 0; corner < corners; ++corner) {
    any |= pass.words[corner];
  }
  pass.charge(corners == 4 ? run(0x80043EA0u, 0x80043EB4u) : run(0x80043D84u, 0x80043D94u));
  if ((any & facts.pulseBit) != 0u) {
    pulseCorners(pass, corners);
    draw(pass);
    return;
  }
  pass.charge(2);
  if ((pass.words[0] & facts.dimBit) == 0u) {
    draw(pass);
    return;
  }
  if (!dimCorners(pass, corners, DimSpan{facts.span})) {
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
    // Crash 2 takes the third vertex from the pair the dispatcher loaded, its z whole; Crash 3 from the GTE.
    Point vertex{.xy = pass.lastXy, .z = pass.lastZ};
    if (pass.title().sway.thirdFromGte) {
      vertex = {.xy = pass.gteData(psx::gte::kVxy2), .z = pass.gteData(psx::gte::kVz2)};
    }
    pass.charge(run(0x80042B20u, 0x80042B30u) + sway(pass, vertex, state).instructions);
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
    pass.charge(run(0x80042C08u, 0x80042C18u) + sway(pass, vertex, state).instructions);
    pass.lastXy = vertex.xy;
    pass.lastZ = vertex.z;
  }
  projectQuad(pass);
}

void drawSwayTriangle(Pass &pass) {
  pass.charge(run(0x80042B44u, 0x80042B50u));
  if (pass.scratchWord(scratch::kModeColour) != 0u) {
    const bool last = (pass.words[2] & kBit19) != 0u;
    pass.charge(3u * 2u + markedCount(pass, 2, VertexBit{kBit19}) + (last ? 2u : 0u));
  }
  swayRecolour(pass, 3);
  drawTriangle(pass);
}

void drawSwayQuad(Pass &pass) {
  pass.charge(run(0x80042C2Cu, 0x80042C38u));
  if (pass.scratchWord(scratch::kModeColour) != 0u) {
    const bool last = (pass.words[3] & kBit19) != 0u;
    pass.charge(4u * 2u + markedCount(pass, 3, VertexBit{kBit19}) + (last ? 2u : 0u));
  }
  swayRecolour(pass, 4);
  drawQuad(pass);
}

} // namespace crash::world
