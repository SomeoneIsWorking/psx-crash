#include "crash3/render/world_underwater.h"

#include "gte_registers.h"
#include "render/world_default.h"
#include "render/world_modes.h"

namespace crash3::world {
namespace {

using namespace crash::world;

// A corner's path through the projector: the sway called for bit 18 or for neither bit, skipped for bit 19 alone.
inline constexpr std::uint32_t kFirstCornerMoved = run(0x800428F8u, 0x80042904u) + run(0x80042908u, 0x80042910u);
inline constexpr std::uint32_t kFirstCornerUnmarked = run(0x800428F0u, 0x80042904u) + run(0x80042908u, 0x80042910u);
inline constexpr std::uint32_t kFirstCornerSkipped = run(0x800428F0u, 0x800428F8u);
inline constexpr std::uint32_t kCornerTest = run(0x8004291Cu, 0x80042924u);
inline constexpr std::uint32_t kCornerMoved = run(0x8004292Cu, 0x80042938u) + run(0x8004293Cu, 0x80042944u);
inline constexpr std::uint32_t kCornerUnmarked = run(0x80042928u, 0x80042938u) + run(0x8004293Cu, 0x80042944u);
inline constexpr std::uint32_t kCornerSkipped = run(0x80042928u, 0x8004292Cu);
inline constexpr std::uint32_t kWriteBack = run(0x80042914u, 0x80042918u);

// One corner swayed into `point` (bit 18 moves it), its sample kept as the corner's shade.
void swayCorner(Pass &pass, const SwayState &state, int corner, Point &point) {
  const std::uint32_t word = pass.words[corner];
  const bool moves = (word & kBit18) != 0u;
  if (!moves && (word & kBit19) != 0u) {
    pass.charge(corner == 0 ? kFirstCornerSkipped : kCornerTest + kCornerSkipped);
    return;
  }
  Point swayed = point;
  const Swayed result = sway(pass, swayed, state);
  pass.storeScratch(scratch::kCornerShades + static_cast<std::uint32_t>(corner) * 4u, result.sample);
  std::uint32_t path = moves ? kFirstCornerMoved : kFirstCornerUnmarked;
  if (corner != 0) {
    path = kCornerTest + (moves ? kCornerMoved : kCornerUnmarked);
  }
  pass.charge(path + result.instructions);
  if (moves) {
    pass.charge(kWriteBack);
    point = swayed;
  }
}

// GTE vector `corner`, swayed in place.
void swayVector(Pass &pass, const SwayState &state, int corner) {
  const std::uint32_t vector = psx::gte::kVxy0 + static_cast<std::uint32_t>(corner) * 2u;
  Point point{.xy = pass.gteData(vector), .z = pass.gteData(vector + 1u)};
  swayCorner(pass, state, corner, point);
  if ((pass.words[corner] & kBit18) != 0u) {
    pass.setGteData(vector, point.xy);
    pass.setGteData(vector + 1u, point.z);
  }
}

// The dispatcher's pair (the triangle's third vertex or the quad's fourth), swayed.
Point swayPair(Pass &pass, const SwayState &state, int corner) {
  Point point{.xy = pass.lastXy, .z = pass.lastZ};
  swayCorner(pass, state, corner, point);
  return point;
}

inline constexpr std::uint32_t kProjectHead = run(0x800428D0u, 0x800428ECu);
inline constexpr std::uint32_t kProjectEnd = run(0x80042984u, 0x80042990u);

// Each corner's colour, its RGBC and IR0 (its shade) loaded, depth-cued unless bit 19. The quad's drawer (0x80042ADC)
// runs the triangle's counts per corner.
void shadeCorners(Pass &pass, int corners) {
  for (int corner = 0; corner < corners; ++corner) {
    const bool last = corner == corners - 1;
    pass.charge(corner == 0 ? run(0x80042994u, 0x800429A4u) : run(0x800429B0u, 0x800429BCu));
    pass.setGteData(psx::gte::kIr0, pass.scratchWord(scratch::kCornerShades + static_cast<std::uint32_t>(corner) * 4u));
    pass.setGteData(psx::gte::kRgbc, pass.colour[corner]);
    if ((pass.words[corner] & kBit19) == 0u) {
      pass.charge(last ? run(0x800429D8u, 0x800429E0u) : run(0x800429A8u, 0x800429ACu));
      pass.command(psx::gte::kDpcs);
      pass.colour[corner] = pass.gteData(psx::gte::kRgb2);
    }
  }
}

} // namespace

void projectUnderwaterTriangle(Pass &pass) {
  pass.charge(kProjectHead + kProjectEnd);
  const SwayState state = swayState(pass);
  swayVector(pass, state, 0);
  swayVector(pass, state, 1);
  const Point third = swayPair(pass, state, 2);
  if ((pass.words[2] & kBit18) != 0u) {
    pass.setGteData(psx::gte::kVxy2, third.xy);
    pass.setGteData(psx::gte::kVz2, third.z);
  }
  projectTriangle(pass);
}

void projectUnderwaterQuad(Pass &pass) {
  pass.charge(kProjectHead + kProjectEnd);
  const SwayState state = swayState(pass);
  swayVector(pass, state, 0);
  swayVector(pass, state, 1);
  swayVector(pass, state, 2);
  const Point fourth = swayPair(pass, state, 3);
  pass.lastXy = fourth.xy;
  pass.lastZ = fourth.z;
  projectQuad(pass);
}

void drawUnderwaterTriangle(Pass &pass) {
  shadeCorners(pass, 3);
  drawTriangle(pass);
}

void drawUnderwaterQuad(Pass &pass) {
  shadeCorners(pass, 4);
  drawQuad(pass);
}

} // namespace crash3::world
