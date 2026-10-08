// H is the GTE near-plane distance: FUN_8003A144 rejects an object unless `H < Z <= 11999`.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>
#include <vector>

class Core;

namespace crash1 {

// The bound lives in the main-RAM global 0x800578D0; its readers load it with `lui` plus a 16-bit
// displacement, so an immediate scan misses them.
inline constexpr std::uint32_t kHorizontalBound = 0x800578D0u;
inline constexpr std::uint32_t kHorizontalBoundWriter = 0x80017820u;
inline constexpr std::uint32_t kHorizontalBoundResend = 0x80026770u;

// The two consumers; 0x2EE0 (12000) is built at 0x8003A248 and the far end is exclusive.
inline constexpr std::uint32_t kHorizontalSubmitter = 0x8001DE78u;
inline constexpr std::uint32_t kHorizontalSubmitterEnd = 0x8001E3D4u;
inline constexpr std::uint32_t kHorizontalNearPlane = 0x8003A144u;
inline constexpr std::uint32_t kHorizontalNearPlaneEnd = 0x8003A76Cu;
inline constexpr std::int32_t kHorizontalFarLimit = 12000;

// gte_init 0x80042B1C publishes CR[26] = 0x3E8 = 1000 once, before any camera mode is chosen.
inline constexpr std::int32_t kRetailInitScreenDistance = 1000;

// The H values the guest can publish; any other value means something moved it.
[[nodiscard]] constexpr bool isRetailHorizontalBound(std::int32_t bound) noexcept {
  return bound == kRetailInitScreenDistance || bound == 500 || bound == 960 || bound == 800 || bound == 460 ||
         bound == 288;
}

// Reads the pre-GTE bound, runs the retail submitter and records what it consumed. Writes no guest state.
class Crash1HorizontalBound final {
public:
  // The retail guest body, run through Lightrec; injected so it can be a plain function pointer.
  using RetailBody = void (*)(Core &);

  // Record the bound the retail submitter is about to read, run it, and check the contract.
  void observeSubmitter(Core &core, const RetailBody &retail);

  // Every distinct bound consumed, in first-seen order; `submissions()` is its denominator.
  [[nodiscard]] const std::vector<std::int32_t> &observedBounds() const {
    return observedBounds_;
  }

  [[nodiscard]] std::uint64_t submissions() const {
    return submissions_;
  }

  // The bound the last submission consumed; zero until one has run.
  [[nodiscard]] std::int32_t lastObservedBound() const {
    return lastObservedBound_;
  }

  // FUN_8003A144's near-plane decision at 0x8003A240..0x8003A24C.
  [[nodiscard]] static constexpr bool insideNearPlane(std::int32_t bound, std::int32_t depth) noexcept {
    return bound < depth && depth < kHorizontalFarLimit;
  }

  // The submitter's light-intensity offset (0x8001E004/0x8001E014); the halving at 0x8001E008/0x8001E010
  // rounds toward zero.
  [[nodiscard]] static constexpr std::int32_t lightIntensityOffset(std::int32_t objectValue,
                                                                   std::int32_t bound) noexcept {
    return (objectValue + 0x800) - (bound / 2);
  }

  // Whether a submission left the bound intact; the owner refuses when false.
  [[nodiscard]] static constexpr bool boundIntact(std::int32_t consumed, std::int32_t after) noexcept {
    return consumed == after;
  }

  // This title's owner, reached from a Core running it through a checked downcast.
  static Crash1HorizontalBound &from(Core &core);

private:
  std::vector<std::int32_t> observedBounds_;
  std::int32_t lastObservedBound_{};
  std::uint64_t submissions_{};
};

void installCrash1HorizontalBound(Core &core);

} // namespace crash1
