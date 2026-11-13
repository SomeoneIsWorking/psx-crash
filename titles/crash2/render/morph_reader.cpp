#include "crash2/render/morph_reader.h"

#include "core.h"
#include "crash2/entry/session.h"
#include "execution_services.h"
#include "native_dispatch.h"
#include "r3000.h"

#include <cstdint>
#include <lucent/log.h>

namespace crash2 {
namespace {

using namespace psx::cpu;

// Names and layouts from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr std::uint32_t kUnlitReader = 0x8003ED14u;
// The morph draw steps (table 0x8005B708) that reach a reader; Gfx_DrawObjects calls them with the packet in $sp.
constexpr std::uint32_t kStep8003F1C8 = 0x8003F1C8u;
constexpr std::uint32_t kStep8003F1D8 = 0x8003F1D8u;
constexpr std::uint32_t kStep8003F410 = 0x8003F410u;
constexpr std::uint32_t kStep80040FC4 = 0x80040FC4u;
constexpr std::uint32_t kStep800417B0 = 0x800417B0u;
// GfxObjectPacket: the two vertex frames, from the object's block +0x20 and +0x24.
constexpr std::uint32_t kPacketFirstFrame = 0x28u;
constexpr std::uint32_t kPacketSecondFrame = 0x2Cu;
constexpr std::uint32_t kPacketObject = 0x10u;
// A vertex frame's box: packed x/y halves, then z.
constexpr std::uint32_t kFrameBoxXy = 0x00u;
constexpr std::uint32_t kFrameBoxZ = 0x04u;
// Scratchpad words of the reader: the vertex cache, both frames' running bytes and the box.
constexpr std::uint32_t kCache = 0x140u;
constexpr std::uint32_t kFirstBytes = 0x110u;
constexpr std::uint32_t kSecondBytes = 0x114u;
constexpr std::uint32_t kBoxXy = 0x118u;
constexpr std::uint32_t kBoxZ = 0x11Cu;
// Strip command bit 26: the vertex is already in the cache.
constexpr std::uint32_t kCachedBit = 1u << 26;
constexpr lucent::Channel morphChannel{"morph"};
// Instructions the retail readers execute on each path, delay slots included, charged to the guest clock: the entry
// to the cache test (0x8003ED14..0x8003ED24), the cache hit (..ED30), the colour tail (0x8003EF60, 7 either way), the
// uncached path's test (ED34, ED38), the raw path's reads (ED3C..ED4C) and its unaligned half (ED50..ED58), the
// delta path's two final loads (EF1C, EF20), and the sum and its two stores (EF24..EF5C).
constexpr std::uint32_t kEntryInstructions = 5;
constexpr std::uint32_t kCachedInstructions = 3;
constexpr std::uint32_t kTailInstructions = 7;
constexpr std::uint32_t kUncachedInstructions = 2;
constexpr std::uint32_t kRawInstructions = 5;
constexpr std::uint32_t kRawUnalignedInstructions = 3;
constexpr std::uint32_t kDeltaLoadInstructions = 2;
constexpr std::uint32_t kSumInstructions = 15;
// A field's next word fetch (0x8003ED88..EDC0), its shift and test (EDC4..EDCC), its delta (EDD0..EDEC) and its two
// byte stores (EDF0, EDF4).
constexpr std::uint32_t kFetchInstructions = 15;
constexpr std::uint32_t kShiftInstructions = 3;
constexpr std::uint32_t kDeltaInstructions = 8;
constexpr std::uint32_t kStoreInstructions = 2;

std::uint32_t sra(std::uint32_t value, std::uint32_t shift) {
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> (shift & 31u));
}

// MIPS lwr and lwl into `reg`.
void lwr(Core &core, Gpr reg, std::uint32_t address) {
  const std::uint32_t shift = (address & 3u) * 8u;
  const std::uint32_t word = core.mem_r32(address & ~3u);
  core.r[reg] = shift == 0u ? word : (core.r[reg] & ~(0xFFFFFFFFu >> shift)) | (word >> shift);
}

void lwl(Core &core, Gpr reg, std::uint32_t address) {
  const std::uint32_t shift = (address & 3u) * 8u;
  const std::uint32_t word = core.mem_r32(address & ~3u);
  core.r[reg] = (core.r[reg] & (0x00FFFFFFu >> shift)) | (word << (24u - shift));
}

// One packed field of both frames' streams: `width` selects its bits, `offset` its running byte, `delta` the
// control word's shared delta, `lead` the instructions before its fetch test (11, 9 and 8 for the three fields).
struct Field {
  std::uint32_t width = 0;
  std::uint32_t offset = 0;
  std::uint32_t delta = 0;
  std::uint32_t lead = 0;
};

// The retail instructions the field took.
std::uint32_t readField(Core &core, const Field &field) {
  std::uint32_t instructions = field.lead + kShiftInstructions + kStoreInstructions;
  std::uint32_t *r = core.r;
  const std::uint32_t offset = field.offset;
  const std::uint32_t delta = field.delta;
  const std::uint32_t bits = (field.width & 7u) + 1u;
  const std::uint32_t shift = (field.width & 7u) ^ 0x1Fu;
  r[kA3] = bits - 8u;
  r[kT4] += bits;
  r[kA0] = sra(r[kT3], shift);
  r[kA1] = sra(r[kSp], shift);
  std::uint32_t consumed = bits;
  if (static_cast<std::int32_t>(r[kT4]) > 0) {
    r[kA0] = r[kT3];
    r[kA1] = r[kSp];
    r[kT3] = core.mem_r32(r[kS4]);
    r[kSp] = core.mem_r32(r[kS5]);
    r[kS4] += 4u;
    r[kS5] += 4u;
    const std::uint32_t rest = (bits - r[kT4]) & 31u;
    r[kA2] = r[kT3] >> rest;
    r[kA0] = sra(r[kA0] | r[kA2], shift);
    r[kA1] = sra(r[kA1] | (r[kSp] >> rest), shift);
    consumed = r[kT4];
    r[kT4] -= 32u;
    instructions += kFetchInstructions;
  }
  r[kT3] <<= consumed & 31u;
  r[kSp] <<= consumed & 31u;
  const std::uint32_t v1 = r[kV1];
  if (r[kA3] != 0u) {
    r[kA0] += delta + static_cast<std::uint32_t>(core.mem_r8s(v1 + kFirstBytes + offset));
    r[kA1] += delta + static_cast<std::uint32_t>(core.mem_r8s(v1 + kSecondBytes + offset));
    instructions += kDeltaInstructions;
  }
  core.mem_w8(v1 + kFirstBytes + offset, static_cast<std::uint8_t>(r[kA0]));
  core.mem_w8(v1 + kSecondBytes + offset, static_cast<std::uint8_t>(r[kA1]));
  return instructions;
}

} // namespace

void MorphReader::weigh(std::uint32_t packet, std::uint32_t quarters) {
  if (quarters == kRetailQuarters) {
    packets_.erase(packet);
    return;
  }
  packets_[packet] = quarters;
}

MorphReader::Weight MorphReader::weightFor(Core &core, std::uint32_t packet) const {
  const auto found = packets_.find(packet);
  if (found == packets_.end()) {
    return {};
  }
  const std::int32_t k = static_cast<std::int32_t>(found->second);
  const std::uint32_t first = core.mem_r32(packet + kPacketFirstFrame);
  const std::uint32_t second = core.mem_r32(packet + kPacketSecondFrame);
  const std::uint32_t a = core.mem_r32(first + kFrameBoxXy);
  const std::uint32_t b = core.mem_r32(second + kFrameBoxXy);
  const auto lane = [k](std::uint32_t from, std::uint32_t to) {
    return ((4 - k) * static_cast<std::int16_t>(from) + k * static_cast<std::int16_t>(to)) >> 2;
  };
  const std::int32_t x = lane(a, b);
  const std::int32_t y = lane(a >> 16, b >> 16);
  const std::int64_t z =
      (static_cast<std::int64_t>(4 - k) * static_cast<std::int32_t>(core.mem_r32(first + kFrameBoxZ)) +
       static_cast<std::int64_t>(k) * static_cast<std::int32_t>(core.mem_r32(second + kFrameBoxZ))) >>
      2;
  return {.quarters = found->second,
          .weighted = true,
          .boxXy = (static_cast<std::uint32_t>(y) << 16) | (static_cast<std::uint32_t>(x) & 0xFFFFu),
          .boxZ = static_cast<std::int32_t>(z)};
}

void MorphReader::read(Core &core) const {
  std::uint32_t *r = core.r;
  const std::uint32_t v1 = r[kV1];
  const std::uint32_t t0 = r[kT0];
  r[kV0] = ((t0 >> 13) & 0x7F8u) + v1;
  const std::uint32_t slot = r[kV0] + kCache;
  std::uint32_t instructions = kEntryInstructions + kTailInstructions;
  if ((t0 & kCachedBit) != 0u) {
    r[kT9] = core.mem_r32(slot);
    r[kT8] = core.mem_r32(slot + 4u);
    instructions += kCachedInstructions;
  } else {
    instructions += kUncachedInstructions + kSumInstructions;
    if (r[kGp] == 0u) {
      instructions += kRawInstructions;
      // Three raw bytes a vertex, read whole.
      r[kT9] = r[kS4] & 2u;
      r[kS4] += 3u;
      r[kS5] += 3u;
      lwr(core, kA0, r[kS4] - 3u);
      lwr(core, kA1, r[kS5] - 3u);
      if (r[kT9] != 0u) {
        lwl(core, kA0, r[kS4]);
        lwl(core, kA1, r[kS5]);
        instructions += kRawUnalignedInstructions;
      }
    } else {
      const std::uint32_t control = core.mem_r32(r[kGp]);
      r[kAt] = control;
      r[kGp] += 4u;
      instructions +=
          readField(core, {.width = control >> 6, .offset = 0u, .delta = sra(control, 25) << 1, .lead = 11});
      instructions += readField(core, {.width = control >> 3, .offset = 2u, .delta = sra(control << 7, 24), .lead = 9});
      instructions += readField(core, {.width = control, .offset = 1u, .delta = sra(control << 15, 24), .lead = 8});
      instructions += kDeltaLoadInstructions;
      r[kA0] = core.mem_r32(v1 + kFirstBytes);
      r[kA1] = core.mem_r32(v1 + kSecondBytes);
    }
    // 0x8003EF28: the two frames' bytes at (4 - k) and k quarters, plus the box at the same weight.
    const std::uint32_t k = current_.quarters;
    const std::uint32_t boxXy = current_.weighted ? current_.boxXy : core.mem_r32(v1 + kBoxXy);
    r[kAt] = core.mem_r32(v1 + kBoxZ);
    const std::uint32_t boxZ = current_.weighted ? static_cast<std::uint32_t>(current_.boxZ) : r[kAt];
    r[kT9] = ((4u - k) * (r[kA0] & r[kT1]) + k * (r[kA1] & r[kT1]) + boxXy) & r[kT2];
    r[kA0] &= 0xFF00u;
    r[kA1] &= 0xFF00u;
    r[kT8] = (((4u - k) * r[kA0] + k * r[kA1]) >> 8) + boxZ;
    core.mem_w32(slot, r[kT9]);
    core.mem_w32(slot + 4u, r[kT8]);
  }
  // The vertex's colour: from the cache's slot (bit 27) or the colour table at $s6.
  const std::uint32_t colour = (t0 >> 7) & 0x1FCu;
  r[kA1] = t0 << 4;
  r[kA0] = static_cast<std::int32_t>(r[kA1]) < 0 ? core.mem_r32(colour + v1 + kCache) : core.mem_r32(colour + r[kS6]);
  accountGuestInstructions(core, instructions);
}

void MorphReader::readVertex(Core *core) {
  session(*core).morphReader.read(*core);
}

template <std::uint32_t Entry> void MorphReader::drawStep(Core *core) {
  Session &state = session(*core);
  MorphReader &reader = state.morphReader;
  // The real draw follows the replays over the same packet addresses; only a replay's own queue weighs them.
  reader.current_ = state.drawReplay.frame().replaying() ? reader.weightFor(*core, core->r[kSp]) : Weight{};
  lucent::debug(morphChannel,
                "step 0x{:08X} packet 0x{:08X} object 0x{:08X} k={} box {:08X} {}",
                Entry,
                core->r[kSp],
                core->mem_r32(core->r[kSp] + kPacketObject),
                reader.current_.quarters,
                reader.current_.boxXy,
                reader.current_.boxZ);
  callOriginalToReturn(*core, Entry, ExecutionBudget::currentTurn(*core), "Crash 2 morph draw step");
  reader.current_ = {};
}

void MorphReader::registerOverrides(Core &core) {
  installNativeOverride(core, kUnlitReader, "Crash 2 morph vertex reader", readVertex);
  installNativeOverride(core, kStep8003F1C8, "Crash 2 morph draw step 0x8003F1C8", drawStep<kStep8003F1C8>);
  installNativeOverride(core, kStep8003F1D8, "Crash 2 morph draw step 0x8003F1D8", drawStep<kStep8003F1D8>);
  installNativeOverride(core, kStep8003F410, "Crash 2 morph draw step 0x8003F410", drawStep<kStep8003F410>);
  installNativeOverride(core, kStep80040FC4, "Crash 2 morph draw step 0x80040FC4", drawStep<kStep80040FC4>);
  installNativeOverride(core, kStep800417B0, "Crash 2 morph draw step 0x800417B0", drawStep<kStep800417B0>);
}

} // namespace crash2
