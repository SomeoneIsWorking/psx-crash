// Widens by moving CR[24] (OFX) alone: H is the GTE near plane and, in Crash 2 and 3, a 2D overlay
// height. The margin comes from the framework's latch through `guest_projection_plan`.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

class Core;
class Game;

namespace crash {

// How the guest reaches `setGeomOffset`; decides whether `$r31 - 8` names a call site.
enum class CentreReach {
  ReturnAddressCallSites, // every reach is a `jal`; `$r31 - 8` names the measured call site
  IndirectCall,           // the leaf is called indirectly; every reach widens
};

// Per-title projection facts, recorded in `titles/<title>/executable.json` under `runtime.projection`.
struct ProjectionTitleFacts {
  // The serial, for log and refusal text.
  std::string_view serial;
  // GTE control register numbers.
  std::uint32_t centreXRegister = 24;        // CR[24] OFX, signed 16.16
  std::uint32_t centreYRegister = 25;        // CR[25] OFY, signed 16.16
  std::uint32_t screenDistanceRegister = 26; // CR[26] H, projection-plane distance

  // The three publication entries, each one guest code reaches by `jal`.
  std::uint32_t projectionInit = 0; // the once-per-boot writer of H, OFX and OFY
  std::uint32_t setGeomOffset = 0;  // the per-frame centre publisher; the widening's latch site
  std::uint32_t setGeomScreen = 0;  // the H publisher; asserted, never changed

  // The retail 4:3 baseline the init publication must produce, checked against the coprocessor registers.
  std::int32_t retailScreenDistance = 0;
  std::int32_t retailCentreX = 0;
  std::int32_t retailCentreY = 0;

  CentreReach centreReach = CentreReach::ReturnAddressCallSites;

  // Call sites of `setGeomOffset` that must not be widened because their `$a0` comes out of the
  // register this owner moves. Empty without a read-back; an `IndirectCall` title cannot declare one.
  const std::uint32_t *passThroughCallSites = nullptr;
  std::size_t passThroughCallSiteCount = 0;

  // Known call sites of `setGeomOffset`; any other is refused. Unused for `IndirectCall`.
  const std::uint32_t *centreCallSites = nullptr;
  std::size_t centreCallSiteCount = 0;
};

// Not `final`: each title derives one.
class GuestProjectionPublication : public GuestWidescreenProjection {
public:
  // The framework's latch, injected through the constructor.
  using Latch = GuestProjectionPlan (*)(Core *, GuestProjectionGeometry);
  // The retail guest body, run through Lightrec; a std::function so the trampoline can bind facts.
  using RetailBody = std::function<void(Core &)>;

  GuestProjectionPublication(const ProjectionTitleFacts &facts, Latch latch);

  // The owner from a Core running it, through a checked downcast; a null or foreign result stops the run.
  static const GuestProjectionPublication &from(const Core &core, std::string_view site);
  static GuestProjectionPublication &from(Core &core, std::string_view site);

  void installSites(Core &core);

  // The latch site, set_geom_offset: widened centre is `retail + margin`, with `retail` the argument
  // register as it stands now, never a remembered value.
  void publishCentre(Core &core, const RetailBody &retail);

  // The projection init writes the registers inline, so the baseline is read back from the coprocessor.
  void publishInitProjection(Core &core, const RetailBody &retail);

  // H is asserted, never modified.
  void observeScreenDistance(Core &core, const RetailBody &retail);

  const GuestProjectionPlan &plan() const {
    return plan_;
  }

  // The H the guest last published, from `$a0` at the measured leaf. Zero until it has run.
  std::int32_t publishedScreenDistance() const {
    return publishedScreenDistance_;
  }

  // True once a centre has been published from the guest's own argument register.
  bool published() const {
    return published_;
  }

  // Centres published, and centres passed through because the argument came out of the coprocessor.
  std::size_t publications() const {
    return publications_;
  }
  std::size_t passThroughs() const {
    return passThroughs_;
  }

  // The centre the guest itself published, as observed.
  struct RetailCentre {
    std::int32_t x = 0;
    std::int32_t y = 0;
  };

  const RetailCentre &retailCentre() const {
    return retail_;
  }

  // Whether the call site is on the pass-through list; only meaningful for `ReturnAddressCallSites`.
  bool passesThrough(std::uint32_t callSite) const;

  // Whether the facts name this call site; the predicate `publishCentre` refuses on.
  bool knowsCallSite(std::uint32_t callSite) const;

  // Whether any pass-through site is declared.
  bool declaresPassThrough() const {
    return facts_.passThroughCallSites != nullptr && facts_.passThroughCallSiteCount != 0;
  }

  const ProjectionTitleFacts &facts() const {
    return facts_;
  }

  // The guest's draw area is the whole display, so this is the display extent it sent through GP1 0xC0.
  GuestProjectionGeometry measuredGeometry(int displayWidth, int displayHeight) const;

  // Whether this is a guest RAM address the owner may read.
  static bool isGuestRam(std::uint32_t address);

  // The plan's centre for a retail centre and margin; a member so a refusal names the title.
  std::int32_t widenedCentreX(std::int32_t retailCentreX, int margin) const;

private:
  GuestProjectionPlan relatch(Core &core);
  // The call site this override was reached from, `$r31 - 8`.
  std::uint32_t observedCallSite(const Core &core, const char *site) const;

  const ProjectionTitleFacts &facts_;
  Latch latch_;
  GuestProjectionPlan plan_;
  RetailCentre retail_;
  std::int32_t publishedScreenDistance_{};
  std::size_t publications_{};
  std::size_t passThroughs_{};
  bool published_{};
};

} // namespace crash
