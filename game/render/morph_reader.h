// morph_reader.h — the morphing models' unlit vertex reader (Crash 2 func_8003ED14, Crash 3 0x8003E0FC, the same
// routine) with quarter weights on the packet's two frames and on a second pair of the same model decoded beside them
// (crash2-decomp and crash3-decomp docs/notes/frame-loop.md, Animation). The lit readers run as guest code at the equal
// weight (plan 0002).
//
// Every register, cache word and delta byte ends as the retail routine leaves it, and the guest clock advances by the
// instructions it runs; with the retail mix the vertex is the retail sum. A replay that draws a pose between two ticks
// names its queued packet and mix; the morph draw steps, entered with the packet in $sp, set the reader's weights and
// the box weighted alike for that packet's draw.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

class Core;

namespace crash {

// Where one title keeps the reader, the routine that opens the packet's frame streams, the morph draw steps that reach
// a reader (entered with the packet in $sp), and the scale of the reader's sum.
struct MorphFacts {
  static constexpr std::size_t kMaxSteps = 8;
  const char *owner;
  std::uint32_t unlitReader;
  std::uint32_t openStreams;
  // A vertex frame's offset to its packed data.
  std::uint32_t frameData;
  // GfxObjectPacket's two vertex frames, from the object's block.
  std::uint32_t packetFirstFrame;
  std::uint32_t packetSecondFrame;
  std::array<std::uint32_t, kMaxSteps> steps;
  std::size_t stepCount;
  // The sum of the frames' bytes at their quarters: x and y shifted left, z right (Crash 2 0 and 8, Crash 3 3 and 5).
  std::uint32_t sumXyShift;
  std::uint32_t sumZShift;
};

class MorphReader {
public:
  explicit MorphReader(const MorphFacts &facts) : facts_(facts) {}

  // The retail readers' equal weight.
  static constexpr std::uint32_t kRetailQuarters = 2;

  // Quarters on the packet's two frames and on a second pair of vertex frames of the same model, four in all.
  struct Mix {
    std::array<std::uint32_t, 4> quarters{kRetailQuarters, kRetailQuarters, 0, 0};
    std::array<std::uint32_t, 2> second{};
    bool operator==(const Mix &) const = default;
  };

  // One of the second pair's streams, read in step with the packet's under the same control words.
  struct Stream {
    std::uint32_t address = 0;
    std::uint32_t bits = 0;
    std::array<std::uint8_t, 4> bytes{};
  };

  static void registerOverrides(Core &core, const MorphFacts &facts);

  // The mix `packet`'s draw takes in a replay, from its queue in the same replay.
  void weigh(std::uint32_t packet, const Mix &mix);

private:
  struct Weight {
    Mix mix;
    bool weighted = false;
    // The box (0x800436F4's halfway of the two frames' boxes) at the same weights.
    std::uint32_t boxXy = 0;
    std::int32_t boxZ = 0;
    std::array<Stream, 2> streams{};
  };

  template <std::size_t Step> static void drawStep(Core *core);
  template <std::size_t... Steps>
  static void registerSteps(Core &core, const MorphFacts &facts, std::index_sequence<Steps...>);
  static void readVertex(Core *core);
  static void openStreams(Core *core);

  void read(Core &core);
  Weight weightFor(Core &core, std::uint32_t packet) const;

  const MorphFacts &facts_;
  std::unordered_map<std::uint32_t, Mix> packets_;
  Weight current_;
};

} // namespace crash
