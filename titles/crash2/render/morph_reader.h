// morph_reader.h — the morphing models' vertex readers, func_8003ED14 and its lit counterpart func_80040D14, with a
// weight of k quarters on the second frame (crash2-decomp docs/notes/frame-loop.md, Animation).
//
// Every register, cache word and delta byte ends as the retail routine leaves it, and the guest clock advances by the
// instructions it runs; with k = 2 the vertex is the retail sum. A replay that draws a quarter pose names its queued
// packet and k; the morph draw steps, entered with the packet in $sp, set the readers' weight and the box weighted
// alike for that packet's draw.
#pragma once

#include <cstdint>
#include <unordered_map>

class Core;

namespace crash2 {

class MorphReader {
public:
  // The retail readers' equal weight.
  static constexpr std::uint32_t kRetailQuarters = 2;

  static void registerOverrides(Core &core);

  // The weight `packet`'s draw takes in a replay, from its queue in the same replay.
  void weigh(std::uint32_t packet, std::uint32_t quarters);

private:
  enum class Lighting : std::uint8_t { Unlit, Lit };

  struct Weight {
    std::uint32_t quarters = kRetailQuarters;
    bool weighted = false;
    // The box (0x800436F4's halfway of the two frames' boxes) at the same weight.
    std::uint32_t boxXy = 0;
    std::int32_t boxZ = 0;
  };

  template <std::uint32_t Entry> static void drawStep(Core *core);
  template <Lighting L> static void readVertex(Core *core);

  void read(Core &core, Lighting lighting) const;
  Weight weightFor(Core &core, std::uint32_t packet) const;

  std::unordered_map<std::uint32_t, std::uint32_t> packets_;
  Weight current_;
};

} // namespace crash2
