// Crash 3 (SCUS-942.44) projection facts; the widening rule is shared `crash::GuestProjectionPublication`.
#pragma once

#include "guest_projection_publication.h"

#include <cstdint>

class Core;

namespace crash3 {

// The three publication entries, all reached by `jal`; no pointer table holds them.
inline constexpr std::uint32_t kProjectionInit = 0x8004F37Cu; // publishes H, OFX and OFY once
inline constexpr std::uint32_t kSetGeomOffset = 0x8004F704u;  // publishes OFX and OFY; the latch site
inline constexpr std::uint32_t kSetGeomScreen = 0x8004F724u;  // publishes H; asserted, never changed

// Call sites of set_geom_offset, recovered as `$r31 - 8`; an unlisted site is refused.
inline constexpr std::uint32_t kCentreCallSites[] = {
    0x8001892Cu, // the camera setup FUN_800188ec, which publishes the retail centre 0 and H 288
    0x80018C04u, // every frame, from FUN_80018a54, with a live camera global as the centre
    0x8001CFC4u, // a transition path, with a sign-corrected halving of a table word as the centre
    0x8001D09Cu, // a transition path, with the centre read back out of CR[24]/CR[25]
};

// The call site that must not be widened: its `$a0` is `lw $a0,184($sp)` at 0x8001D094, a slot written
// by the `jal 0x8004F6E4` at 0x8001CF8C, so it comes from the register this owner moves.
inline constexpr std::uint32_t kPassThroughCallSites[] = {0x8001D09Cu};

inline constexpr std::size_t kCentreCallSiteCount = sizeof(kCentreCallSites) / sizeof(kCentreCallSites[0]);
inline constexpr std::size_t kPassThroughCallSiteCount =
    sizeof(kPassThroughCallSites) / sizeof(kPassThroughCallSites[0]);
static_assert(kPassThroughCallSiteCount < kCentreCallSiteCount,
              "a title cannot pass through every centre call site and still widen anything");

// The retail 4:3 baseline the image publishes.
inline constexpr std::int32_t kRetailScreenDistance = 1000; // 0x3E8, CR[26]
inline constexpr std::int32_t kRetailCentreX = 0;           // CR[24]
inline constexpr std::int32_t kRetailCentreY = 0;           // CR[25]

// The projection-plane distance global. H is held because FUN_8003c3d0 [0x8003C3D0,0x8003C994) rejects
// a vertex unless H < Z < 12000, and FUN_80016634 [0x80016634,0x80016CE8) sizes a 2D overlay from H.
inline constexpr std::uint32_t kScreenDistanceCache = 0x80065D54u;

class Crash3Widescreen final : public crash::GuestProjectionPublication {
public:
  using GuestProjectionPublication::GuestProjectionPublication;

  static const crash::ProjectionTitleFacts &facts();
};

} // namespace crash3
