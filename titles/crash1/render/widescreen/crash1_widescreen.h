// Crash 1 (SCUS-949.00) projection facts; the widening rule is shared `crash::GuestProjectionPublication`.
#pragma once

#include "guest_projection_publication.h"

#include <cstdint>

class Core;

namespace crash1 {

inline constexpr std::uint32_t kProjectionInit = 0x80042B1Cu; // writes OFX, OFY and H once, from $zero
inline constexpr std::uint32_t kSetGeomOffset = 0x80042F8Cu;  // writes OFX and OFY; the widening's site
inline constexpr std::uint32_t kSetGeomScreen = 0x80042FACu;  // writes H; asserted, never changed

// Exactly two `jal`s target set_geom_offset, and no pointer table holds its address: 0x8001783C in the
// camera setup FUN_80017790 and 0x80017F00 in the per-frame matrix update CoreLoop calls at 0x800123BC.
inline constexpr crash::CentreReach kCentreReach = crash::CentreReach::ReturnAddressCallSites;

inline constexpr std::uint32_t kCentreCallSites[] = {0x8001783Cu, 0x80017F00u};

// The retail 4:3 baseline gte_init publishes, compared against the guest's coprocessor registers.
inline constexpr std::int32_t kRetailScreenDistance = 1000; // 0x3E8, CR[26]
inline constexpr std::int32_t kRetailCentreX = 0;           // CR[24]
inline constexpr std::int32_t kRetailCentreY = 0;           // CR[25]

// Declares no pass-through call site: the image has no `cfc2` read of CR[24]/CR[25].
class Crash1Widescreen final : public crash::GuestProjectionPublication {
public:
  using GuestProjectionPublication::GuestProjectionPublication;

  static const crash::ProjectionTitleFacts &facts();
};

} // namespace crash1