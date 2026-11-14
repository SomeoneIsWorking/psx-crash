// world_modes.h — the scene modes both titles' world renderers have (sway, pulse, glow) and the pieces their other
// modes build on: marked corners, dimming by depth, a lamp's reach and the sway of one vertex. Title constants come
// from the pass's `Title` (its `sway`, `pulse` and `glow` facts).
#pragma once

#include "render/world_pass.h"

#include <array>
#include <cstdint>

namespace crash::world {

// A vertex's vector, or the corner's taken for it.
struct Point {
  std::uint32_t xy = 0;
  std::uint32_t z = 0;
};

// A vertex word's mode bit.
struct VertexBit {
  std::uint32_t mask = 0;
};

// How far past the title's near depth a corner dims to black.
struct DimSpan {
  std::uint32_t depth = 0;
};

// The corners whose vertex has `bit`, depth-cued toward the far colour by the IR0 already set.
void cueMarked(Pass &pass, int corners, VertexBit bit);
std::uint32_t markedCount(const Pass &pass, int corners, VertexBit bit);
// Corners nearer than the title's near depth go black, those up to `span` past it toward black; whether any colour is
// left (a quad's first depth is the one its projector kept).
bool dimCorners(Pass &pass, int corners, DimSpan span);

// A lamp's reach left at a vertex, less how far it is along z, y and x (`flat` off before x).
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

// A lamp's position and how far it reaches (`flat` taken off before the last axis).
struct Lamp {
  std::uint32_t xy = 0;
  std::uint32_t z = 0;
  std::uint32_t start = 0;
  std::uint32_t flat = 0;
};

inline constexpr std::uint32_t kLampFlat = 0x320u;
inline constexpr std::uint32_t kLampFull = 0x1000u;

Reach reach(const Point &vertex, const Lamp &lamp);
// The lit corners' words, the upcoming kind and the depth kept in the scratchpad.
void keepLit(Pass &pass, int corners);
// A corner's vector: the quad's first as its projector kept it, its others in GTE vectors 1, 2 and 0.
std::array<Point, 4> litVectors(Pass &pass, int corners);

// The sway of one vertex: the clock, scale and phase it reads, and what it took.
struct SwayState {
  std::uint32_t clock = 0;
  std::uint32_t scale = 0;
  std::uint32_t phase = 0;
};
struct Swayed {
  std::uint32_t instructions = 0;
  // The sine table's halfword that moved the pair, unscaled (Crash 3's water keeps it as the corner's shade).
  std::uint32_t sample = 0;
};
// The upcoming entry kept, and the sway's inputs read.
SwayState swayState(Pass &pass);
Swayed sway(Pass &pass, Point &vertex, const SwayState &state);

// Sway (flag 8): bit 18's vertices moved by a sine; bit 19's corners recoloured.
void projectSwayTriangle(Pass &pass);
void projectSwayQuad(Pass &pass);
void drawSwayTriangle(Pass &pass);
void drawSwayQuad(Pass &pass);
// Pulse (flag 0x40): marked corners pulse with the clock; else, the first vertex's dim bit set, corners dim by depth.
void drawPulseTriangle(Pass &pass);
void drawPulseQuad(Pass &pass);
// Glow: corners dimmed by their distance from two objects; far from both, black and flat.
void drawGlowTriangle(Pass &pass);
void drawGlowQuad(Pass &pass);

} // namespace crash::world
