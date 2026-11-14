#include "crash3/render/water_grid.h"

#include "core.h"
#include "emit_memory.h"
#include "execution_services.h"
#include "game.h"
#include "gte.h"
#include "gte_registers.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "render/world_pass.h"
#include "render/world_screen.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace crash3 {
namespace {

using namespace psx::cpu;
using crash::world::Cull;
using crash::world::negative;
using crash::world::run;
using crash::world::ScreenTest;
using crash::world::ScreenVerdict;
using crash::world::sra;
using psx::gte::ProjectedVertex;

inline constexpr std::string_view kOwner = "Crash 3 water grid";
inline constexpr std::uint32_t kEntry = 0x80043FFCu;
// The vertex routine's two entries (kept at scratchpad 0x380) and the row routines' return addresses (0x388).
inline constexpr std::uint32_t kVertexAcross = 0x80044674u;
inline constexpr std::uint32_t kVertexAlong = 0x800446B0u;
inline constexpr std::uint32_t kRowReturnNear = 0x800441B8u;
inline constexpr std::uint32_t kRowReturnFar = 0x80044464u;
// Each row's first and last column, a byte pair a row; the far half reads it backwards from its end.
inline constexpr std::uint32_t kRowColumns = 0x8005D31Cu;
inline constexpr std::uint32_t kRowColumnsFar = 0x8005D37Eu;

inline constexpr std::uint32_t kFrameSize = 0x40u;
inline constexpr std::uint32_t kFrameNext = 0x08u;
inline constexpr std::uint32_t kFrameOt = 0x20u;
inline constexpr std::uint32_t kPhases = 0xE0u;

// Scene words.
inline constexpr std::uint32_t kSceneCamera = 0x00u;
inline constexpr std::uint32_t kSceneCentre = 0x0Cu;
inline constexpr std::uint32_t kSceneLevel = 0x9Cu;
inline constexpr std::uint32_t kScenePhase = 0xA0u;
inline constexpr std::uint32_t kSceneFrameNow = 0xA4u;
inline constexpr std::uint32_t kSceneFrameNext = 0xA8u;
inline constexpr std::uint32_t kSceneHeights = 0xACu;
inline constexpr std::uint32_t kSceneTexture = 0xB4u;
inline constexpr std::uint32_t kSceneFrames = 0xB8u;
// The camera's heading and the centre's position.
inline constexpr std::uint32_t kCameraHeading = 0x10u;
inline constexpr std::uint32_t kCentreX = 0x14u;
inline constexpr std::uint32_t kCentreY = 0x18u;
inline constexpr std::uint32_t kCentreZ = 0x1Cu;

// Scratchpad words, as offsets from its base.
namespace scratch {
inline constexpr std::uint32_t kBase = 0x1F800000u;
inline constexpr std::uint32_t kSavedSp = 0x000u;
// Per column: the vertex's colour, its texture coordinate and valid flag, its screen word and depth.
inline constexpr std::uint32_t kColour = 0x060u;
inline constexpr std::uint32_t kTexel = 0x124u;
inline constexpr std::uint32_t kValid = 0x126u;
inline constexpr std::uint32_t kScreen = 0x1E8u;
inline constexpr std::uint32_t kDepth = 0x2ACu;
// The cell's texture words: CLUT and A's coordinate, page and B's, C's and D's; the coordinate base.
inline constexpr std::uint32_t kTexture0 = 0x370u;
inline constexpr std::uint32_t kTexture1 = 0x374u;
inline constexpr std::uint32_t kTexture2 = 0x378u;
inline constexpr std::uint32_t kTexelBase = 0x37Cu;
inline constexpr std::uint32_t kVertexRoutine = 0x380u;
inline constexpr std::uint32_t kSavedRa = 0x384u;
inline constexpr std::uint32_t kRowRa = 0x388u;
inline constexpr std::uint32_t kScreenEnd = 0x38Cu;
inline constexpr std::uint32_t kFirstScreen = 0x390u;
inline constexpr std::uint32_t kLastColour = 0x394u;
inline constexpr std::uint32_t kBlend = 0x398u;
inline constexpr std::uint32_t kBytes = 0x400u;
} // namespace scratch

inline constexpr std::uint32_t kColumns = 49u;
inline constexpr std::uint32_t kRowStep = 0x100u;
inline constexpr std::uint32_t kDeepest = 0x770u;
inline constexpr std::uint32_t kLastSlot = 0x1FFCu;
inline constexpr std::uint32_t kQuadCommand = 0x3E000000u;
inline constexpr std::uint32_t kTriangleCommand = 0x36000000u;
inline constexpr std::uint32_t kQuadWords = 13u;
inline constexpr std::uint32_t kTriangleWords = 10u;

// How a cell's corners are read from the height map: along the rows or across them, on the near or the far half.
enum class Facing : std::uint8_t {
  AlongNear,
  AlongFar,
  AcrossNear,
  AcrossFar,
};

// The retail walk, or the walk over the canvas's columns that draws only what the retail one culls.
enum class Sweep : std::uint8_t {
  Retail,
  Margin,
};

struct Vertex {
  std::uint32_t colour = 0;
  std::uint32_t texel = 0;
};

[[noreturn]] void fault(std::string_view what, std::uint32_t value) {
  lucent::error("crash3-water", "{}: {} 0x{:08X}", kOwner, what, value);
  std::abort();
}

class Grid {
public:
  explicit Grid(Core &core)
      : core_(core), gte_(core.gte()), memory_(core),
        screen_(psx::present::recordCanvasColumns(core.game->guestDisplay.plan(), crash::world::kScreenColumns)) {}

  void draw();

private:
  std::uint32_t word(std::uint32_t address) {
    return core_.mem_r32(address);
  }
  void store(std::uint32_t address, std::uint32_t value) {
    core_.mem_w32(address, value);
  }
  std::uint32_t scratchWord(std::uint32_t offset) {
    return core_.mem_r32(scratch::kBase + offset);
  }
  std::uint32_t scratchHalf(std::uint32_t offset) {
    return core_.mem_r16(scratch::kBase + offset);
  }
  void storeScratch(std::uint32_t offset, std::uint32_t value) {
    core_.mem_w32(scratch::kBase + offset, value);
  }
  void storeScratchHalf(std::uint32_t offset, std::uint32_t value) {
    core_.mem_w16(scratch::kBase + offset, static_cast<std::uint16_t>(value));
  }
  void charge(std::uint32_t instructions) {
    if (sweep_ == Sweep::Retail) {
      charged_ += instructions;
    }
  }
  bool across() const {
    return facing_ == Facing::AcrossNear || facing_ == Facing::AcrossFar;
  }

  void walk();
  void row(std::uint32_t first, std::uint32_t last, std::uint32_t z);
  Vertex vertex(std::uint32_t column, std::uint32_t z, std::uint32_t mask);
  std::uint32_t heightAt(std::uint32_t x, std::uint32_t z);
  void drawCell(bool margin);
  void drawQuad();
  void drawTriangles(std::uint32_t split);
  std::uint32_t slot();
  void storeTriangle(std::uint32_t colour,
                     const ProjectedVertex &a,
                     std::uint32_t texture0,
                     std::uint32_t colour1,
                     const ProjectedVertex &b,
                     std::uint32_t texture1,
                     std::uint32_t colour2,
                     const ProjectedVertex &c,
                     std::uint32_t texture2,
                     std::uint32_t slotAddress);

  Core &core_;
  Gte &gte_;
  psx::present::EmitMemory memory_;
  ScreenTest screen_;
  Sweep sweep_ = Sweep::Retail;
  std::uint32_t charged_ = 0;

  // Registers of the retail code: the frame ($fp) and next primitive ($s7), the height map ($t7), the two texture
  // frames' tables ($t2, $t3), the map offsets ($t0, $t1, one shifted to the map's row), the grid's origin ($t5, $t6)
  // and the water level ($at).
  Facing facing_ = Facing::AlongNear;
  std::uint32_t frame_ = 0;
  std::uint32_t next_ = 0;
  std::uint32_t heights_ = 0;
  std::uint32_t frameNow_ = 0;
  std::uint32_t frameNext_ = 0;
  std::uint32_t mapX_ = 0;
  std::uint32_t mapZ_ = 0;
  std::uint32_t originX_ = 0;
  std::uint32_t originZ_ = 0;
  std::uint32_t level_ = 0;

  // The walk: the column the next row starts from at the latest ($sp), the cell ($s0, $s1) and the colours of its
  // corners A, B, C ($s2, $s3, $s4; D's at scratchpad 0x394).
  std::uint32_t hint_ = 0;
  std::uint32_t column_ = 0;
  std::uint32_t z_ = 0;
  std::uint32_t colourA_ = 0;
  std::uint32_t colourB_ = 0;
  std::uint32_t colourC_ = 0;

  // The GTE's SXY FIFO with each entry's place, A's screen (scratchpad 0x390), and the places of the column cache.
  std::array<ProjectedVertex, 3> fifo_{};
  ProjectedVertex first_{};
  std::array<ProjectedVertex, kColumns> cached_{};
};

void Grid::draw() {
  const std::uint32_t clock = core_.r[kA0];
  frame_ = core_.r[kA1];
  const std::uint32_t scene = core_.r[kA2];

  charge(run(0x80043FFCu, 0x8004412Cu));
  gte_.writeControl(psx::gte::kZsf3, 0x555u);
  gte_.writeControl(psx::gte::kZsf4, 0x400u);
  const std::uint32_t phase = clock % kPhases;
  storeScratch(scratch::kSavedRa, core_.r[kRa]);
  storeScratch(scratch::kSavedSp, core_.r[kSp] - kFrameSize);
  heights_ = word(scene + kSceneHeights);
  const std::uint32_t blend = phase & 7u;
  const std::uint32_t frames = scene + ((phase >> 1) & 0xFFFCu);
  frameNext_ = word(frames + kSceneFrames + 4u);
  frameNow_ = word(frames + kSceneFrames);
  store(scene + kScenePhase, blend);
  store(scene + kSceneFrameNow, frameNow_);
  store(scene + kSceneFrameNext, frameNext_);
  const std::uint32_t texture = word(scene + kSceneTexture);
  storeScratch(scratch::kBlend, blend << 9);
  const std::uint32_t texture0 = word(texture + 4u);
  storeScratch(scratch::kTexture0, texture0);
  storeScratchHalf(scratch::kTexelBase, texture0);
  // Semi-transparency mode 1 in the page word.
  storeScratch(scratch::kTexture1, (word(texture + 8u) & 0xFF9FFFFFu) | 0x00200000u);
  storeScratch(scratch::kTexture2, word(texture + 12u));
  const std::uint32_t centre = word(scene + kSceneCentre);
  next_ = word(frame_ + kFrameNext);
  const std::uint32_t x = word(centre + kCentreX) - 0x6400u;
  const std::uint32_t z = word(centre + kCentreZ) - 0x6000u;
  mapX_ = x >> 10;
  mapZ_ = z >> 10;
  originX_ = sra(0u - 0x6000u - (x & 0x3FFu), 2);
  originZ_ = sra(0u - 0x6000u - (z & 0x3FFu), 2);
  level_ = sra(word(scene + kSceneLevel) - word(centre + kCentreY), 3);
  gte_.writeControl(psx::gte::kTranslationX, 0);
  gte_.writeControl(psx::gte::kTranslationY, 0);
  gte_.writeControl(psx::gte::kTranslationZ, 0);
  storeScratch(scratch::kScreenEnd, crash::world::kScreenEnd);

  // The heading's quadrant, a quarter turn centred on each axis: bit 11 the far half, bit 10 across the rows.
  const std::uint32_t heading = word(word(scene + kSceneCamera) + kCameraHeading) + 0x200u;
  const bool far = negative(heading << 20);
  const bool acrossRows = negative(heading << 21);
  facing_ = far ? (acrossRows ? Facing::AcrossFar : Facing::AlongFar)
                : (acrossRows ? Facing::AcrossNear : Facing::AlongNear);
  if (acrossRows) {
    mapX_ <<= 8;
  } else {
    mapZ_ <<= 8;
  }
  storeScratch(scratch::kVertexRoutine, acrossRows ? kVertexAcross : kVertexAlong);
  storeScratch(scratch::kRowRa, far ? kRowReturnFar : kRowReturnNear);

  walk();
  if (screen_.widened()) {
    // The margin's cells drawn as the walk over the canvas finds them; everything but the frame's packets put back.
    std::array<std::uint32_t, scratch::kBytes / 4u> pad{};
    for (std::uint32_t i = 0; i < pad.size(); ++i) {
      pad[i] = scratchWord(i * 4u);
    }
    const GteRawState gte = gte_.saveRaw();
    sweep_ = Sweep::Margin;
    walk();
    gte_.restoreRaw(gte);
    for (std::uint32_t i = 0; i < pad.size(); ++i) {
      storeScratch(i * 4u, pad[i]);
    }
    sweep_ = Sweep::Retail;
  }
  charge(run(0x800441C8u, 0x80044208u));
  store(frame_ + kFrameNext, next_);
  accountGuestInstructions(core_, charged_);
}

void Grid::walk() {
  for (std::uint32_t column = 0; column < kColumns; ++column) {
    storeScratch(scratch::kTexel + column * 4u, 0);
  }
  charge(kColumns * run(0x80044130u, 0x8004413Cu));
  // The quadrant's branches, the same count on each of the four paths.
  charge(run(0x80044140u, 0x80044154u) + run(0x80044158u, 0x8004415Cu) + run(0x80044160u, 0x80044174u) +
         run(0x80044190u, 0x80044198u));
  for (std::uint32_t entry = 0; entry < fifo_.size(); ++entry) {
    fifo_[entry] = gte_.projected(entry);
  }
  if (facing_ == Facing::AlongNear || facing_ == Facing::AcrossNear) {
    hint_ = 0;
    for (std::uint32_t z = 0; z < 0x1900u; z += kRowStep) {
      const std::uint32_t pair = kRowColumns + (z >> 7);
      charge(run(0x8004419Cu, 0x800441B4u));
      row(core_.mem_r8(pair), core_.mem_r8(pair + 1u), z);
      charge(run(0x800441B8u, 0x800441C4u));
    }
    return;
  }
  hint_ = 0x30u;
  for (std::uint32_t z = 0x3000u; z > 0x1700u; z -= kRowStep) {
    const std::uint32_t pair = kRowColumnsFar - (z >> 7);
    charge(run(0x80044448u, 0x80044460u));
    row(core_.mem_r8(pair + 1u), core_.mem_r8(pair), z);
    charge(run(0x80044464u, 0x80044470u));
  }
  charge(run(0x80044474u, 0x80044478u));
}

// One row (0x8004420C near, 0x8004447C far, the far one mirrored): the cells are walked from `first` toward `last`;
// the culled cells before the first drawn one move the next row's start, and the first culled after it ends the row.
void Grid::row(std::uint32_t first, std::uint32_t last, std::uint32_t z) {
  const bool far = facing_ == Facing::AlongFar || facing_ == Facing::AcrossFar;
  const auto step = static_cast<std::int32_t>(far ? -1 : 1);
  const std::uint32_t nextZ = far ? z - kRowStep : z + kRowStep;
  // Columns left to walk toward `last`, in the walk's direction.
  const auto ahead = [&](std::uint32_t column) {
    return static_cast<std::int32_t>(last - column) * step;
  };

  charge(run(0x8004420Cu, 0x80044218u));
  std::uint32_t column = first;
  if (static_cast<std::int32_t>(first - hint_) * step < 0) {
    charge(run(0x8004421Cu, 0x80044220u));
    column = hint_;
    // Past the centre column the row starts one short of the hint, whose cache entry is stale.
    const std::uint32_t pivot = far ? 0x19u : 0x17u;
    if (static_cast<std::int32_t>(hint_ - pivot) * step >= 0) {
      charge(run(0x80044224u, 0x80044234u));
      hint_ -= static_cast<std::uint32_t>(step);
      column = hint_;
      storeScratchHalf(scratch::kValid + hint_ * 4u, 0);
    }
  }
  z_ = z;
  charge(run(0x80044238u, 0x80044248u));
  Vertex corner = vertex(column, z, ~0u);
  colourA_ = corner.colour;
  storeScratchHalf(scratch::kTexture0, corner.texel);
  charge(run(0x8004424Cu, 0x80044264u));
  corner = vertex(column, nextZ, 0);
  colourB_ = corner.colour;
  storeScratchHalf(scratch::kTexture1, corner.texel);
  charge(run(0x80044268u, 0x80044278u));
  if (ahead(column) <= 0) {
    charge(run(0x800443F4u, 0x80044400u));
    return;
  }
  column += static_cast<std::uint32_t>(step);

  bool drawing = false;
  while (true) {
    column_ = column;
    charge(run(0x8004427Cu, 0x8004428Cu));
    corner = vertex(column, z, ~0u);
    colourC_ = corner.colour;
    storeScratchHalf(scratch::kTexture2, corner.texel);
    first_ = fifo_[0];
    storeScratch(scratch::kFirstScreen, first_.word());
    charge(run(0x80044290u, 0x800442ACu));
    corner = vertex(column, nextZ, 0);
    storeScratch(scratch::kLastColour, corner.colour);
    storeScratchHalf(scratch::kTexture2 + 2u, corner.texel);
    charge(run(0x800442B0u, 0x80044300u));
    gte_.execute(psx::gte::kAvsz4);
    const std::array<std::uint32_t, 4> screens{first_.word(), fifo_[0].word(), fifo_[1].word(), fifo_[2].word()};
    const ScreenVerdict verdict = screen_.test(screens.data(), 4);
    const bool kept = verdict.cull == Cull::Visible && (sweep_ == Sweep::Margin || !verdict.margin);
    if (verdict.cull != Cull::Rows) {
      charge(run(0x80044304u, 0x80044308u));
    }
    if (!kept && drawing) {
      charge(run(0x800443F4u, 0x80044400u));
      return;
    }
    if (kept) {
      charge(run(0x800443C8u, 0x800443CCu));
      drawCell(verdict.margin);
      charge(run(0x800443D0u, 0x800443F0u));
      drawing = true;
    } else {
      charge(run(0x8004430Cu, 0x80044330u));
      hint_ = column;
    }
    colourA_ = colourC_;
    colourB_ = scratchWord(scratch::kLastColour);
    storeScratchHalf(scratch::kTexture0, scratchHalf(scratch::kTexture2));
    storeScratchHalf(scratch::kTexture1, scratchHalf(scratch::kTexture2 + 2u));
    if (ahead(column) <= 0) {
      if (!drawing) {
        charge(run(0x80044334u, 0x80044338u));
      }
      charge(run(0x800443F4u, 0x80044400u));
      return;
    }
    column += static_cast<std::uint32_t>(step);
  }
}

// The height map's byte at (x, z), z in its row's place; the last row reads the first.
std::uint32_t Grid::heightAt(std::uint32_t x, std::uint32_t z) {
  const std::uint32_t rowBits = z & 0xFF00u;
  std::uint32_t index = (x & 0xFFu) | rowBits;
  if (rowBits == 0xFF00u) {
    charge(1);
    index &= 0xFFu;
  }
  return core_.mem_r8(heights_ + index);
}

// The vertex routine (0x80044674 across the rows, 0x800446B0 along them): a grid point's height from the map and the
// two texture frames' tables blended by the GTE, projected, its texture coordinate blended the same way and its
// colour shaded by distance; kept per column. With `mask` set a point the column already holds is reused, the GTE's
// FIFO shifted by hand as RTPS would (SZ0 kept).
Vertex Grid::vertex(std::uint32_t column, std::uint32_t z, std::uint32_t mask) {
  if (column >= kColumns) {
    fault("column outside the cache", column);
  }
  const std::uint32_t entry = column * 4u;
  const bool acrossRows = across();
  charge(acrossRows ? run(0x80044674u, 0x8004468Cu) : run(0x800446B0u, 0x800446C8u));
  if ((static_cast<std::uint32_t>(core_.mem_r16s(scratch::kBase + scratch::kValid + entry)) & mask) != 0u) {
    charge(run(0x80044844u, 0x80044874u));
    gte_.writeData(psx::gte::kSxy0, fifo_[1].word());
    gte_.writeData(psx::gte::kSz1, gte_.readData(psx::gte::kSz2));
    gte_.writeData(psx::gte::kSxy1, fifo_[2].word());
    gte_.writeData(psx::gte::kSz2, gte_.readData(psx::gte::kSz3));
    gte_.writeData(psx::gte::kSxy2, scratchWord(scratch::kScreen + entry));
    gte_.writeData(psx::gte::kSz3, scratchWord(scratch::kDepth + entry));
    fifo_ = {fifo_[1], fifo_[2], cached_[column]};
    return {.colour = scratchWord(scratch::kColour + entry), .texel = scratchHalf(scratch::kTexel + entry)};
  }
  std::uint32_t x = 0;
  std::uint32_t depth = 0;
  std::uint32_t mapX = 0;
  std::uint32_t mapZ = 0;
  if (acrossRows) {
    charge(run(0x80044690u, 0x800446ACu));
    x = z + originX_;
    depth = (column << 8) + originZ_;
    mapX = (z + mapX_) >> 8;
    mapZ = (column + mapZ_) << 8;
  } else {
    charge(run(0x800446CCu, 0x800446DCu));
    x = (column << 8) + originX_;
    depth = z + originZ_;
    mapX = column + mapX_;
    mapZ = z + mapZ_;
  }
  gte_.writeData(psx::gte::kVz0, depth);
  charge(run(0x800446E0u, 0x800446E4u));
  std::uint32_t height = 0x10u;
  if (heights_ != 0u) {
    charge(run(0x800446E8u, 0x800446F8u) + run(0x80044700u, 0x80044704u));
    height = heightAt(mapX, mapZ);
  }
  charge(run(0x80044708u, 0x800447C0u));
  storeScratchHalf(scratch::kValid + entry, 0xFFFFu);
  // The two frames' words for this 32 x 32 cell: a colour-encoded texture coordinate, the height in the top byte.
  const std::uint32_t cell = ((mapX & 0x1Fu) | ((mapZ & 0x1F00u) >> 3)) << 2;
  const std::uint32_t now = word(frameNow_ + cell);
  const std::uint32_t then = word(frameNext_ + cell);
  const std::uint32_t blend = scratchWord(scratch::kBlend);
  gte_.writeData(psx::gte::kRgbc, now);
  gte_.writeControl(psx::gte::kFarRed, (then << 4) & 0xFF0u);
  gte_.writeControl(psx::gte::kFarGreen, (then >> 4) & 0xFF0u);
  gte_.writeControl(psx::gte::kFarBlue, (then >> 12) & 0xFF0u);
  gte_.writeData(psx::gte::kIr0, blend);
  const std::uint32_t heightNow = sra(now, 24);
  const std::uint32_t heightThen = sra(then, 24);
  gte_.execute(psx::gte::kDpcs);
  const std::uint32_t wave = (heightNow << 3) + sra(blend * (heightThen - heightNow), 9);
  const std::uint32_t lifted = (height & 0x1Fu) * wave;
  const std::uint32_t blended = gte_.readData(psx::gte::kRgb2);
  std::uint32_t shade = (blended >> 12) & 0xFF0u;
  const std::uint32_t texel = blended + scratchHalf(scratch::kTexelBase);
  const std::uint32_t y = sra(lifted, 7) + level_;
  gte_.writeData(psx::gte::kVxy0, (x & 0xFFFFu) | (y << 17));
  std::uint32_t far = depth;
  if (negative(far)) {
    charge(1);
    far = 0u - far;
  }
  charge(run(0x800447C8u, 0x800447CCu));
  std::uint32_t side = x;
  if (negative(side)) {
    charge(1);
    side = 0u - side;
  }
  gte_.execute(psx::gte::kRtps);
  charge(run(0x800447D4u, 0x800447E0u));
  // Distance as the larger axis plus a quarter of the smaller.
  std::uint32_t distance = side;
  std::uint32_t nearer = sra(far, 2);
  if (negative(side - far)) {
    charge(run(0x800447E4u, 0x800447E8u));
    nearer = sra(side, 2);
    distance = far;
  }
  charge(run(0x800447ECu, 0x800447F8u));
  distance = distance + nearer - 0x800u;
  std::uint32_t colour = 0;
  bool grey = true;
  if (negative(distance)) {
    charge(run(0x800447FCu, 0x80044800u));
    shade >>= 5;
  } else {
    charge(run(0x80044804u, 0x8004480Cu));
    const std::uint32_t left = 0x1000u - distance;
    if (static_cast<std::int32_t>(left) <= 0) {
      charge(run(0x8004482Cu, 0x8004482Cu));
      grey = false;
    } else {
      charge(run(0x80044810u, 0x80044814u));
      shade = sra(left * shade, 17);
    }
  }
  if (grey) {
    charge(run(0x80044818u, 0x80044828u));
    colour = shade | (shade << 8) | (shade << 16);
  }
  charge(run(0x80044830u, 0x80044840u));
  const ProjectedVertex projected = gte_.projected(2);
  fifo_ = {fifo_[1], fifo_[2], projected};
  cached_[column] = projected;
  storeScratch(scratch::kScreen + entry, projected.word());
  storeScratch(scratch::kDepth + entry, gte_.readData(psx::gte::kSz3));
  storeScratch(scratch::kColour + entry, colour);
  storeScratchHalf(scratch::kTexel + entry, texel);
  return {.colour = colour, .texel = texel};
}

// The cell drawer at $t4 (0x80044878 along near, 0x800448BC along far, 0x80044908 across near, 0x80044960 across
// far): the cell's map byte either splits it into triangles (bit 7; bits 6 and 5, turned by the facing, pick which)
// or names it drawn (bits 7..5 nonzero) or not; with no height map every cell is a quad.
void Grid::drawCell(bool margin) {
  if (sweep_ == Sweep::Margin && !margin) {
    return;
  }
  if (heights_ == 0u) {
    charge(run(0x80044878u, 0x8004487Cu) + run(0x800449B0u, 0x800449C4u));
    drawQuad();
    return;
  }
  const std::uint32_t column = column_;
  std::uint32_t split = 0;
  std::uint32_t kind = 0;
  bool splits = false;
  switch (facing_) {
  case Facing::AlongNear: {
    charge(run(0x80044878u, 0x80044894u));
    const std::uint32_t cell = heightAt(column + mapX_, z_ + mapZ_);
    charge(run(0x8004489Cu, 0x800448B0u));
    splits = (cell & 0x80u) != 0u;
    split = cell;
    kind = cell >> 5;
    if (!splits) {
      charge(run(0x800448B4u, 0x800448B8u));
    }
    break;
  }
  case Facing::AlongFar: {
    charge(run(0x800448BCu, 0x800448C0u) + run(0x800448C4u, 0x800448E0u));
    const std::uint32_t cell = heightAt(column + mapX_ + 1u, z_ + mapZ_ - kRowStep);
    charge(run(0x800448E8u, 0x800448FCu));
    splits = (cell & 0x80u) != 0u;
    split = cell + 0x40u;
    kind = cell >> 5;
    if (!splits) {
      charge(run(0x80044900u, 0x80044904u));
    }
    break;
  }
  case Facing::AcrossNear: {
    charge(run(0x80044908u, 0x8004490Cu) + run(0x80044910u, 0x80044934u));
    const std::uint32_t cell = heightAt(((z_ + mapX_) >> 8) + 1u, ((column + mapZ_) << 8) - kRowStep) & 0xE0u;
    charge(run(0x8004493Cu, 0x80044954u));
    splits = (cell & 0x80u) != 0u;
    split = 0x80u - cell;
    kind = cell >> 5;
    if (!splits) {
      charge(run(0x80044958u, 0x8004495Cu));
    }
    break;
  }
  case Facing::AcrossFar: {
    charge(run(0x80044960u, 0x80044964u) + run(0x80044968u, 0x80044984u));
    const std::uint32_t cell = heightAt((z_ + mapX_) >> 8, (column + mapZ_) << 8) & 0xE0u;
    charge(run(0x8004498Cu, 0x800449A4u));
    splits = (cell & 0x80u) != 0u;
    split = 0xC0u - cell;
    kind = cell >> 5;
    if (!splits) {
      charge(run(0x800449A8u, 0x800449A8u));
    }
    break;
  }
  }
  if (splits) {
    drawTriangles(split);
    return;
  }
  charge(run(0x800449ACu, 0x800449B0u));
  if (kind == 0u) {
    charge(run(0x80044A60u, 0x80044A64u));
    return;
  }
  charge(run(0x800449B4u, 0x800449C4u));
  drawQuad();
}

// The ordering table slot of the cell's AVSZ4 depth, clamped to the table.
std::uint32_t Grid::slot() {
  const std::uint32_t depth = gte_.readData(psx::gte::kMac0) >> 15;
  std::uint32_t offset = kDeepest - depth;
  if (negative(offset)) {
    charge(1);
    offset = 0;
  } else {
    offset <<= 2;
  }
  charge(3);
  if (static_cast<std::int32_t>(offset - kLastSlot) > 0) {
    charge(1);
    offset = kLastSlot;
  }
  return frame_ + offset + kFrameOt;
}

// A semi-transparent GT4 over A, B, C, D; none when every corner is black.
void Grid::drawQuad() {
  const std::uint32_t colourD = scratchWord(scratch::kLastColour);
  if ((colourA_ | colourB_ | colourC_ | colourD) == 0u) {
    charge(run(0x80044A60u, 0x80044A64u));
    return;
  }
  charge(run(0x800449C8u, 0x800449E4u));
  const std::uint32_t slotAddress = slot();
  charge(run(0x800449FCu, 0x80044A5Cu));
  const std::uint32_t texture2 = scratchWord(scratch::kTexture2);
  store(next_ + 0x04u, colourA_ | kQuadCommand);
  memory_.storeVertex(next_ + 0x08u, first_);
  store(next_ + 0x0Cu, scratchWord(scratch::kTexture0));
  store(next_ + 0x10u, colourB_);
  memory_.storeVertex(next_ + 0x14u, fifo_[0]);
  store(next_ + 0x18u, scratchWord(scratch::kTexture1));
  store(next_ + 0x1Cu, colourC_);
  memory_.storeVertex(next_ + 0x20u, fifo_[1]);
  store(next_ + 0x24u, texture2);
  store(next_ + 0x28u, colourD);
  memory_.storeVertex(next_ + 0x2Cu, fifo_[2]);
  store(next_ + 0x30u, texture2 >> 16);
  crash::world::linkAtHead(core_, slotAddress, next_, kQuadWords);
}

void Grid::storeTriangle(std::uint32_t colour,
                         const ProjectedVertex &a,
                         std::uint32_t texture0,
                         std::uint32_t colour1,
                         const ProjectedVertex &b,
                         std::uint32_t texture1,
                         std::uint32_t colour2,
                         const ProjectedVertex &c,
                         std::uint32_t texture2,
                         std::uint32_t slotAddress) {
  charge(run(0x80044C7Cu, 0x80044C9Cu));
  store(next_ + 0x04u, colour | kTriangleCommand);
  memory_.storeVertex(next_ + 0x08u, a);
  store(next_ + 0x0Cu, texture0);
  store(next_ + 0x10u, colour1);
  memory_.storeVertex(next_ + 0x14u, b);
  store(next_ + 0x18u, texture1);
  store(next_ + 0x1Cu, colour2);
  memory_.storeVertex(next_ + 0x20u, c);
  store(next_ + 0x24u, texture2);
  crash::world::linkAtHead(core_, slotAddress, next_, kTriangleWords);
}

// Half the cell as a semi-transparent GT3: BCD, ABD, ABC or ACD by bits 6 and 5 of `split`. The ACD test ORs the
// shifted bits instead of A's colour, so ACD is drawn even when black.
void Grid::drawTriangles(std::uint32_t split) {
  const std::uint32_t colourD = scratchWord(scratch::kLastColour);
  const std::uint32_t texture0 = scratchWord(scratch::kTexture0);
  const std::uint32_t texture1 = scratchWord(scratch::kTexture1);
  const std::uint32_t texture2 = scratchWord(scratch::kTexture2);
  // The CLUT and page halves with B's or C's coordinate, and D's alone.
  const std::uint32_t clutB = (texture0 & 0xFFFF0000u) | (texture1 & 0xFFFFu);
  const std::uint32_t pageC = (texture1 & 0xFFFF0000u) | (texture2 & 0xFFFFu);
  const std::uint32_t texelD = texture2 >> 16;
  charge(run(0x80044A68u, 0x80044A70u));
  const bool bit6 = negative(split << 25);
  const bool bit5 = negative(split << 26);
  if (bit6 && bit5) {
    charge(run(0x80044B84u, 0x80044B88u) + run(0x80044BFCu, 0x80044C0Cu) + run(0x80044C10u, 0x80044C34u));
    const std::uint32_t slotAddress = slot();
    charge(run(0x80044C4Cu, 0x80044C78u));
    storeTriangle(colourA_, first_, texture0, colourC_, fifo_[1], pageC, colourD, fifo_[2], texelD, slotAddress);
    return;
  }
  if (bit6) {
    charge(run(0x80044B84u, 0x80044B88u) + run(0x80044B8Cu, 0x80044B94u));
    if ((colourA_ | colourB_ | colourC_) == 0u) {
      charge(run(0x80044A60u, 0x80044A64u));
      return;
    }
    charge(run(0x80044B98u, 0x80044BB4u));
    const std::uint32_t slotAddress = slot();
    charge(run(0x80044BCCu, 0x80044BF8u));
    storeTriangle(colourA_, first_, texture0, colourB_, fifo_[0], texture1, colourC_, fifo_[1], texture2, slotAddress);
    return;
  }
  charge(run(0x80044A74u, 0x80044A78u));
  if (bit5) {
    charge(run(0x80044B08u, 0x80044B14u));
    if ((colourA_ | colourB_ | colourD) == 0u) {
      charge(run(0x80044A60u, 0x80044A64u));
      return;
    }
    charge(run(0x80044B18u, 0x80044B34u));
    const std::uint32_t slotAddress = slot();
    charge(run(0x80044B4Cu, 0x80044B80u));
    storeTriangle(colourA_, first_, texture0, colourB_, fifo_[0], texture1, colourD, fifo_[2], texelD, slotAddress);
    return;
  }
  charge(run(0x80044A7Cu, 0x80044A88u));
  if ((colourB_ | colourC_ | colourD) == 0u) {
    charge(run(0x80044A60u, 0x80044A64u));
    return;
  }
  charge(run(0x80044A8Cu, 0x80044ABCu));
  const std::uint32_t slotAddress = slot();
  charge(run(0x80044AD4u, 0x80044B04u));
  storeTriangle(colourB_, fifo_[0], clutB, colourC_, fifo_[1], pageC, colourD, fifo_[2], texelD, slotAddress);
}

void draw(Core *core) {
  Grid(*core).draw();
}

} // namespace

void WaterGrid::registerOverrides(Core &core) {
  installNativeOverride(core, kEntry, kOwner, draw, NativeReturn::Void);
}

} // namespace crash3
