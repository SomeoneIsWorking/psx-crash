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

inline constexpr std::uint32_t kLampReach = 0x1320u;

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

void lampColours(Pass &pass, int corners) {
  keepLit(pass, corners);
  const std::array<Point, 4> points = litVectors(pass, corners);
  for (int corner = 0; corner < corners; ++corner) {
    pass.storeScratch(scratch::kCameraZ, pass.colour[corner]);
    lampCorner(pass, points[corner], pass.scratchWord(scratch::kLitWords + static_cast<std::uint32_t>(corner) * 4u));
    pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
  }
}

// ---- Waves: bit 18's vertices moved along y by the scratchpad's wave table.

std::uint32_t waved(Pass &pass, std::uint32_t xy, std::uint32_t phase) {
  const std::uint32_t index = (((xy >> 16) + phase + xy) >> 4) & 0xFu;
  return xy + (static_cast<std::uint32_t>(pass.core().mem_r8(scratch::kBase + scratch::kWaves + index)) << 16);
}

void waveVector(Pass &pass, std::uint32_t vector, std::uint32_t phase) {
  pass.setGteData(vector, waved(pass, pass.gteData(vector), phase));
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
              run(0x800449FCu, 0x80044A00u) + 2u * markedCount(pass, 3, VertexBit{kBit19}));
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kTint));
  cueMarked(pass, 3, VertexBit{kBit19});
  drawTriangle(pass);
}

void drawTintQuad(Pass &pass) {
  pass.charge(run(0x80044A04u, 0x80044A14u) + run(0x80044A20u, 0x80044A2Cu) + run(0x80044A38u, 0x80044A44u) +
              run(0x80044A50u, 0x80044A5Cu) + run(0x80044A68u, 0x80044A6Cu) +
              2u * markedCount(pass, 4, VertexBit{kBit19}));
  pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kTint));
  cueMarked(pass, 4, VertexBit{kBit19});
  drawQuad(pass);
}

namespace {

inline constexpr std::uint32_t kWaveSpan = 0x3A0u;

// All-bit-19 polygons dim by depth.
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
  if (!dimCorners(pass, corners, DimSpan{kWaveSpan})) {
    return;
  }
  draw(pass);
}

} // namespace

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

} // namespace crash2::world
