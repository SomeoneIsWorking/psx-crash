// Crash 2 (SCUS-941.54) projection facts; the widening rule is shared `crash::GuestProjectionPublication`.
#pragma once

#include "guest_projection_publication.h"

#include <cstdint>

class Core;

namespace crash2 {

// The three publication entries, all reached by `jal`; no pointer table holds them.
inline constexpr std::uint32_t kProjectionInit = 0x8004EC30u; // publishes H, OFX and OFY once
inline constexpr std::uint32_t kSetGeomOffset = 0x8004EFE8u;  // publishes OFX and OFY; the latch site
inline constexpr std::uint32_t kSetGeomScreen = 0x8004F008u;  // publishes H; asserted, never changed

// Call sites of set_geom_offset, recovered as `$r31 - 8`; an unlisted site is refused.
inline constexpr std::uint32_t kCentreCallSites[] = {
    0x800179CCu, // the camera setup FUN_8001798c, which publishes the retail centre 0 and H 288
    0x80017F70u, // every frame, from FUN_80017bc4, with a live camera global as the centre
};

inline constexpr std::size_t kCentreCallSiteCount = sizeof(kCentreCallSites) / sizeof(kCentreCallSites[0]);

// The retail 4:3 baseline the image publishes.
inline constexpr std::int32_t kRetailScreenDistance = 1000; // 0x3E8, CR[26]
inline constexpr std::int32_t kRetailCentreX = 0;           // CR[24]
inline constexpr std::int32_t kRetailCentreY = 0;           // CR[25]

// The projection-plane distance global. H is held because FUN_8003d3cc [0x8003D3CC,0x8003DA18) rejects
// a vertex unless H < Z < 12000 and FUN_8001645c [0x8001645C,0x80016A68) sizes a 2D overlay from H.
inline constexpr std::uint32_t kScreenDistanceCache = 0x80060884u;

class Crash2Widescreen final : public crash::GuestProjectionPublication {
public:
  using GuestProjectionPublication::GuestProjectionPublication;

  static const crash::ProjectionTitleFacts &facts();
};

} // namespace crash2
