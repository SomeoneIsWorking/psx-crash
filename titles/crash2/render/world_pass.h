// world_pass.h — Gfx_DrawWorlds' polygon on its way through the scene's mode, and the retail steps the modes share
// (crash2-decomp docs/notes/world-renderer.md). Every scratchpad word, primitive word and GTE register ends as the
// retail code leaves it; the guest clock is charged the instructions the retail path runs.
#pragma once

#include "core.h"
#include "emit_memory.h"
#include "gte.h"
#include "screen_cull.h"

#include <array>
#include <cstdint>

namespace crash2::world {

// The instructions in the retail run [first, last], delay slots included.
constexpr std::uint32_t run(std::uint32_t first, std::uint32_t last) {
  return (last - first) / 4u + 1u;
}

// SRA and the sign bit of a guest word.
constexpr std::uint32_t sra(std::uint32_t value, std::uint32_t shift) {
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> (shift & 31u));
}
constexpr bool negative(std::uint32_t value) {
  return static_cast<std::int32_t>(value) < 0;
}

// Scratchpad words of the renderer, as offsets from its base.
namespace scratch {
inline constexpr std::uint32_t kBase = 0x1F800000u;
inline constexpr std::uint32_t kSavedSp = 0x000u;
inline constexpr std::uint32_t kFogShift = 0x008u;
inline constexpr std::uint32_t kFogStart = 0x010u;
inline constexpr std::uint32_t kClock = 0x01Cu;
inline constexpr std::uint32_t kSwayScale = 0x060u;
inline constexpr std::uint32_t kDrawTriangle = 0x064u;
inline constexpr std::uint32_t kDrawQuad = 0x068u;
inline constexpr std::uint32_t kDrained = 0x06Cu;
inline constexpr std::uint32_t kUpcoming = 0x078u;
inline constexpr std::uint32_t kLamp1Xy = 0x07Cu;
inline constexpr std::uint32_t kLamp1Z = 0x080u;
inline constexpr std::uint32_t kPulse = 0x084u;
inline constexpr std::uint32_t kLamp2Xy = 0x088u;
inline constexpr std::uint32_t kLamp2Z = 0x08Cu;
inline constexpr std::uint32_t kShade = 0x090u;
inline constexpr std::uint32_t kLightX = 0x094u;
inline constexpr std::uint32_t kLightZ = 0x098u;
inline constexpr std::uint32_t kFirst = 0x09Cu;
inline constexpr std::uint32_t kProjectTriangle = 0x0A8u;
inline constexpr std::uint32_t kProjectQuad = 0x0ACu;
inline constexpr std::uint32_t kLitWords = 0x0B0u;
inline constexpr std::uint32_t kCameraX = 0x0C0u;
inline constexpr std::uint32_t kCameraY = 0x0C4u;
// The camera's z, which the lit modes reuse for a corner's colour.
inline constexpr std::uint32_t kCameraZ = 0x0C8u;
inline constexpr std::uint32_t kPhase = 0x0CCu;
inline constexpr std::uint32_t kDepthShifts = 0x0D0u;
inline constexpr std::uint32_t kSwayShift = 0x0F0u;
inline constexpr std::uint32_t kLightTexture = 0x0F4u;
inline constexpr std::uint32_t kWaves = 0x100u;
inline constexpr std::uint32_t kQuadFirstXy = 0x178u;
inline constexpr std::uint32_t kQuadFirstZ = 0x17Cu;
inline constexpr std::uint32_t kRotated = 0x184u;
inline constexpr std::uint32_t kLitDepth = 0x188u;
inline constexpr std::uint32_t kModeColour = 0x1A8u;
inline constexpr std::uint32_t kQuadFirstVxy = 0x1B4u;
inline constexpr std::uint32_t kQuadFirstVz = 0x1B8u;
inline constexpr std::uint32_t kVertexMask = 0x1D8u;
inline constexpr std::uint32_t kPairMask = 0x1DCu;
inline constexpr std::uint32_t kLampFar = 0x1F0u;
inline constexpr std::uint32_t kTintFar = 0x1FCu;
inline constexpr std::uint32_t kLampFull = 0x208u;
inline constexpr std::uint32_t kTint = 0x214u;
inline constexpr std::uint32_t kScreenEnd = 0x21Cu;
inline constexpr std::uint32_t kWorlds = 0x280u;
} // namespace scratch

// A world block in the scratchpad (WorldBlock): its offset and tables.
namespace block {
inline constexpr std::uint32_t kOffsetXy = 0x00u;
inline constexpr std::uint32_t kOffsetZ = 0x04u;
inline constexpr std::uint32_t kTriangles = 0x08u;
inline constexpr std::uint32_t kQuads = 0x0Cu;
inline constexpr std::uint32_t kVertices = 0x10u;
inline constexpr std::uint32_t kColours = 0x14u;
inline constexpr std::uint32_t kAnimations = 0x18u;
inline constexpr std::uint32_t kTextures = 0x1Cu;
} // namespace block

// A sort list entry's kind (bits 11 and 12).
inline constexpr std::uint32_t kKindMask = 0x1800u;
inline constexpr std::uint32_t kKindTriangle = 0x0000u;
inline constexpr std::uint32_t kKindQuadFirst = 0x0800u;
inline constexpr std::uint32_t kKindQuadLast = 0x1000u;
inline constexpr std::uint32_t kKindQuad = 0x1800u;

// The VXY mask of a vertex word and the pair mask that keeps x's carry out of y.
inline constexpr std::uint32_t kVertexMask = 0xFFF0FFF0u;
inline constexpr std::uint32_t kPairMask = 0xFFFEFFFFu;
// The retail screen: 512 columns, 217 rows, packed as the projectors subtract it.
inline constexpr std::uint32_t kScreenEnd = 0x00D90200u;
inline constexpr int kScreenColumns = 0x200;
// A vertex word's bits the modes read.
inline constexpr std::uint32_t kBit18 = 0x40000u;
inline constexpr std::uint32_t kBit19 = 0x80000u;
// The ordering table's deepest polygon slot.
inline constexpr std::uint32_t kDeepest = 0x770u;

// A decoded sort list entry: its world block (scratchpad address), kind and record words.
struct Record {
  std::uint32_t world = 0;
  std::uint32_t kind = 0;
  std::uint32_t first = 0;
  std::uint32_t second = 0;
};

class Pass;
using Step = void (*)(Pass &);

// Which retail path found a texture: fixed, an animation's frame, a depth level of that frame, or a depth level.
enum class TexturePath : std::uint8_t {
  Fixed,
  Frame,
  FrameLevel,
  Level,
};

struct TextureLookup {
  std::uint32_t address = 0;
  TexturePath path = TexturePath::Fixed;
};

// How a polygon left the mode: skipped (0x80042390) or linked by the draw tail (0x80042948).
enum class Ending : std::uint8_t {
  Skipped,
  Linked,
};

// Which edge test rejected a polygon: the rows' (bit 31, tested first) or the columns' (bit 15).
enum class Cull : std::uint8_t {
  Visible,
  Rows,
  Columns,
};

class Pass {
public:
  Pass(Core &core, psx::present::CullWindow columns)
      : core_(core), gte_(core.gte()), memory_(core), columns_(columns) {}

  Core &core() {
    return core_;
  }
  psx::cpu::Gte &gte() {
    return gte_;
  }

  std::uint32_t scratchWord(std::uint32_t offset) {
    return core_.mem_r32(scratch::kBase + offset);
  }
  void storeScratch(std::uint32_t offset, std::uint32_t value) {
    core_.mem_w32(scratch::kBase + offset, value);
  }
  std::uint32_t word(std::uint32_t address) {
    return core_.mem_r32(address);
  }
  void store(std::uint32_t address, std::uint32_t value) {
    core_.mem_w32(address, value);
  }
  void storeHalf(std::uint32_t address, std::uint32_t value) {
    core_.mem_w16(address, static_cast<std::uint16_t>(value));
  }
  std::int32_t signedHalf(std::uint32_t address) {
    return core_.mem_r16s(address);
  }

  // SWC2 of SXY0..2: the word with its projected place.
  void storeScreen(std::uint32_t address, std::uint32_t entry) {
    memory_.storeVertex(address, gte_.projected(entry));
  }
  // A screen word moved from one guest word to another (LW then SW), its place with it.
  void copyScreen(std::uint32_t to, std::uint32_t from) {
    memory_.storeVertex(to, memory_.loadVertex(from));
  }
  std::uint32_t screen(std::uint32_t entry) {
    return gte_.projected(entry).word();
  }
  std::uint32_t gteData(std::uint32_t reg) {
    return gte_.readData(reg);
  }
  void setGteData(std::uint32_t reg, std::uint32_t value) {
    gte_.writeData(reg, value);
  }
  void setGteControl(std::uint32_t reg, std::uint32_t value) {
    gte_.writeControl(reg, value);
  }
  void command(std::uint32_t instruction) {
    gte_.execute(instruction);
  }
  // CTC2 of the three far colour components.
  void setFarColour(std::uint32_t red, std::uint32_t green, std::uint32_t blue);

  void charge(std::uint32_t instructions) {
    if (!margin_) {
      instructions_ += instructions;
    }
  }
  std::uint32_t charged() const {
    return instructions_;
  }

  // Off the screen: every corner left of its first column, right of its last, or past its last row. The rows and the
  // retail columns are the packed test of 0x800424B8; a wider canvas moves only the column edges. Charges the column
  // test's branch when the rows pass.
  Cull offScreen(const std::uint32_t *screens, int corners);
  // A polygon only the wider canvas keeps: the guest clock is charged as the retail cull left it.
  bool margin() const {
    return margin_;
  }
  void endPolygon() {
    margin_ = false;
  }

  // The vertex's colour from the current world's table (word and halfword of its vertex).
  std::uint32_t vertexColour(int corner);

  // 0x80042424..: the next entry's record, decoded while the GTE projects ($s4, $s3, $gp and the kind in $t3).
  void decodeNext();
  void decode(std::uint32_t entry, Record &record);

  // The ordering table slot of the depth (SZ1 + SZ2 + SZ3) >> 5, slot 0 farthest; returns 1 when clamped.
  std::uint32_t depthSlot();

  // The material's texture (fixed, an animation's frame by the clock, or a depth level's) and which path found it.
  TextureLookup lookupTexture();
  // The same, charged as the default drawers find it.
  std::uint32_t textureAddress();
  // The texture's three words, turned when the scratchpad's rotated flag is set (the second triangle of a quad).
  std::array<std::uint32_t, 3> textureWords(std::uint32_t texture);
  std::array<std::uint32_t, 3> triangleTexture();

  // 0x80042948: the primitive linked into the table built from the top down, `words` long; ends the polygon.
  void linkPrimitive(std::uint32_t words);
  // The slots from below the lowest linked down to `target`, each pointing to the one above; the slots stored.
  std::uint32_t relinkDown(std::uint32_t target);
  // The primitive at `next`, `words` long, put at the head of the lowest slot.
  void appendAtLowest(std::uint32_t words);
  // The slots below the lowest linked, down to `past` (excluded), each pointing to the one above.
  void linkDown(std::uint32_t past);

  // The polygon in flight. Registers of the retail code: world block $s4 (then the next one's), the tables, the
  // material $s5, the vertex words ($at, $a3, $t0, $a0) and halfwords, colours ($t5, $t6, $t7, $a1), the pair the
  // dispatcher loaded last ($v0, $a2: the triangle's third vertex or the quad's fourth), the depth ($ra) and slot
  // ($v0).
  Record current;
  std::uint32_t offsetXy = 0;
  std::uint32_t offsetZ = 0;
  std::uint32_t vertices = 0;
  std::uint32_t colours = 0;
  std::uint32_t animations = 0;
  std::uint32_t textures = 0;
  std::uint32_t material = 0;
  std::array<std::uint32_t, 4> words{};
  std::array<std::uint32_t, 4> halves{};
  std::array<std::uint32_t, 4> colour{};
  std::uint32_t lastXy = 0;
  std::uint32_t lastZ = 0;
  std::uint32_t depth = 0;
  std::uint32_t slot = 0;
  Ending ending = Ending::Skipped;
  // The mode's drawers (scratchpad 0x64 and 0x68).
  Step drawTriangleStep = nullptr;
  Step drawQuadStep = nullptr;

  // The upcoming entry ($t3 as the dispatcher reads it) and its record once decoded.
  std::uint32_t upcomingEntry = 0;
  Record upcoming;

  // The frame: ordering table ($fp), next primitive ($s7), lowest slot linked ($t4) and what it points to ($t1).
  std::uint32_t ot = 0;
  std::uint32_t next = 0;
  std::uint32_t lowest = 0;
  std::uint32_t head = 0;

private:
  Core &core_;
  psx::cpu::Gte &gte_;
  psx::present::EmitMemory memory_;
  psx::present::CullWindow columns_;
  std::uint32_t instructions_ = 0;
  bool margin_ = false;
};

} // namespace crash2::world
