#include "render/morph_reader.h"

#include "core.h"
#include "execution_services.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "runtime/session.h"

#include <cstdint>
#include <lucent/log.h>
#include <span>

namespace crash {
namespace {

using namespace psx::cpu;

// The layouts both titles share (crash2-decomp and crash3-decomp docs/notes/frame-loop.md).
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

// What the packet's streams did for a field, which a second-pair stream repeats.
struct FieldStep {
  bool fetch = false;
  std::uint32_t rest = 0;
  std::uint32_t consumed = 0;
  bool delta = false;
};

void readStreamField(Core &core, MorphReader::Stream &stream, const Field &field, const FieldStep &step) {
  const std::uint32_t shift = (field.width & 7u) ^ 0x1Fu;
  std::uint32_t value = sra(stream.bits, shift);
  if (step.fetch) {
    const std::uint32_t held = stream.bits;
    stream.bits = core.mem_r32(stream.address);
    stream.address += 4u;
    value = sra(held | (stream.bits >> step.rest), shift);
  }
  stream.bits <<= step.consumed & 31u;
  if (step.delta) {
    value += field.delta + static_cast<std::uint32_t>(static_cast<std::int8_t>(stream.bytes[field.offset]));
  }
  stream.bytes[field.offset] = static_cast<std::uint8_t>(value);
}

// The retail instructions the field took.
std::uint32_t readField(Core &core, const Field &field, std::span<MorphReader::Stream> streams) {
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
  const bool fetch = static_cast<std::int32_t>(r[kT4]) > 0;
  const std::uint32_t rest = (bits - r[kT4]) & 31u;
  if (fetch) {
    r[kA0] = r[kT3];
    r[kA1] = r[kSp];
    r[kT3] = core.mem_r32(r[kS4]);
    r[kSp] = core.mem_r32(r[kS5]);
    r[kS4] += 4u;
    r[kS5] += 4u;
    r[kA2] = r[kT3] >> rest;
    r[kA0] = sra(r[kA0] | r[kA2], shift);
    r[kA1] = sra(r[kA1] | (r[kSp] >> rest), shift);
    consumed = r[kT4];
    r[kT4] -= 32u;
    instructions += kFetchInstructions;
  }
  for (MorphReader::Stream &stream : streams) {
    readStreamField(core, stream, field, {.fetch = fetch, .rest = rest, .consumed = consumed, .delta = r[kA3] != 0u});
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

void MorphReader::weigh(std::uint32_t packet, const Mix &mix) {
  if (mix == Mix{}) {
    packets_.erase(packet);
    return;
  }
  packets_[packet] = mix;
}

MorphReader::Weight MorphReader::weightFor(Core &core, std::uint32_t packet) const {
  const auto found = packets_.find(packet);
  if (found == packets_.end()) {
    return {};
  }
  const Mix &mix = found->second;
  const std::array<std::uint32_t, 4> frames{core.mem_r32(packet + facts_.packetFirstFrame),
                                            core.mem_r32(packet + facts_.packetSecondFrame),
                                            mix.second[0],
                                            mix.second[1]};
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int64_t z = 0;
  for (std::size_t i = 0; i < frames.size(); ++i) {
    const auto w = static_cast<std::int32_t>(mix.quarters[i]);
    if (w == 0) {
      continue;
    }
    const std::uint32_t xy = core.mem_r32(frames[i] + kFrameBoxXy);
    x += w * static_cast<std::int16_t>(xy);
    y += w * static_cast<std::int16_t>(xy >> 16);
    z += static_cast<std::int64_t>(w) * static_cast<std::int32_t>(core.mem_r32(frames[i] + kFrameBoxZ));
  }
  x >>= 2;
  y >>= 2;
  z >>= 2;
  return {.mix = mix,
          .weighted = true,
          .boxXy = (static_cast<std::uint32_t>(y) << 16) | (static_cast<std::uint32_t>(x) & 0xFFFFu),
          .boxZ = static_cast<std::int32_t>(z)};
}

void MorphReader::openStreams(Core *core) {
  MorphReader &reader = session(*core).morphReader();
  callOriginalToReturn(*core, reader.facts_.openStreams, ExecutionBudget::currentTurn(*core), reader.facts_.owner);
  Weight &weight = reader.current_;
  for (std::size_t i = 0; i < weight.streams.size(); ++i) {
    const std::uint32_t frame = weight.mix.second[i];
    weight.streams[i] =
        frame == 0u ? Stream{} : Stream{.address = frame + core->mem_r32(frame + reader.facts_.frameData)};
  }
}

void MorphReader::read(Core &core) {
  std::uint32_t *r = core.r;
  const std::uint32_t v1 = r[kV1];
  const std::uint32_t t0 = r[kT0];
  r[kV0] = ((t0 >> 13) & 0x7F8u) + v1;
  const std::uint32_t slot = r[kV0] + kCache;
  std::uint32_t instructions = kEntryInstructions + kTailInstructions;
  // The second pair's streams, when the mix weighs it.
  const std::span<Stream> streams =
      current_.mix.second[0] != 0u ? std::span<Stream>(current_.streams) : std::span<Stream>();
  std::array<std::uint32_t, 2> second{};
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
      for (std::size_t i = 0; i < streams.size(); ++i) {
        const std::uint32_t at = streams[i].address;
        second[i] = core.mem_r8(at) | core.mem_r8(at + 1u) << 8 | core.mem_r8(at + 2u) << 16;
        streams[i].address += 3u;
      }
    } else {
      const std::uint32_t control = core.mem_r32(r[kGp]);
      r[kAt] = control;
      r[kGp] += 4u;
      instructions +=
          readField(core, {.width = control >> 6, .offset = 0u, .delta = sra(control, 25) << 1, .lead = 11}, streams);
      instructions +=
          readField(core, {.width = control >> 3, .offset = 2u, .delta = sra(control << 7, 24), .lead = 9}, streams);
      instructions +=
          readField(core, {.width = control, .offset = 1u, .delta = sra(control << 15, 24), .lead = 8}, streams);
      instructions += kDeltaLoadInstructions;
      r[kA0] = core.mem_r32(v1 + kFirstBytes);
      r[kA1] = core.mem_r32(v1 + kSecondBytes);
      for (std::size_t i = 0; i < streams.size(); ++i) {
        const std::array<std::uint8_t, 4> &bytes = streams[i].bytes;
        second[i] = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
      }
    }
    // 0x8003EF28: the frames' bytes at their quarters, plus the box at the same weights.
    const std::array<std::uint32_t, 4> &w = current_.mix.quarters;
    const std::uint32_t boxXy = current_.weighted ? current_.boxXy : core.mem_r32(v1 + kBoxXy);
    r[kAt] = core.mem_r32(v1 + kBoxZ);
    const std::uint32_t boxZ = current_.weighted ? static_cast<std::uint32_t>(current_.boxZ) : r[kAt];
    const std::uint32_t lanes = r[kT1];
    r[kT9] =
        (((w[0] * (r[kA0] & lanes) + w[1] * (r[kA1] & lanes) + w[2] * (second[0] & lanes) + w[3] * (second[1] & lanes))
          << facts_.sumXyShift) +
         boxXy) &
        r[kT2];
    r[kA0] &= 0xFF00u;
    r[kA1] &= 0xFF00u;
    r[kT8] = ((w[0] * r[kA0] + w[1] * r[kA1] + w[2] * (second[0] & 0xFF00u) + w[3] * (second[1] & 0xFF00u)) >>
              facts_.sumZShift) +
             boxZ;
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
  session(*core).morphReader().read(*core);
}

template <std::size_t Step> void MorphReader::drawStep(Core *core) {
  Session &state = session(*core);
  MorphReader &reader = state.morphReader();
  const std::uint32_t entry = reader.facts_.steps[Step];
  // The real draw follows the replays over the same packet addresses; only a replay's own queue weighs them.
  reader.current_ = state.drawReplay().frame().replaying() ? reader.weightFor(*core, core->r[kSp]) : Weight{};
  lucent::debug(morphChannel,
                "step 0x{:08X} packet 0x{:08X} object 0x{:08X} quarters {} {} {} {} box {:08X} {}",
                entry,
                core->r[kSp],
                core->mem_r32(core->r[kSp] + kPacketObject),
                reader.current_.mix.quarters[0],
                reader.current_.mix.quarters[1],
                reader.current_.mix.quarters[2],
                reader.current_.mix.quarters[3],
                reader.current_.boxXy,
                reader.current_.boxZ);
  callOriginalToReturn(*core, entry, ExecutionBudget::currentTurn(*core), reader.facts_.owner);
  reader.current_ = {};
}

template <std::size_t... Steps>
void MorphReader::registerSteps(Core &core, const MorphFacts &facts, std::index_sequence<Steps...>) {
  ((Steps < facts.stepCount ? installNativeOverride(core, facts.steps[Steps], facts.owner, drawStep<Steps>) : void()),
   ...);
}

void MorphReader::registerOverrides(Core &core, const MorphFacts &facts) {
  installNativeOverride(core, facts.unlitReader, facts.owner, readVertex);
  installNativeOverride(core, facts.openStreams, facts.owner, openStreams);
  registerSteps(core, facts, std::make_index_sequence<MorphFacts::kMaxSteps>{});
}

} // namespace crash
