#include "guest_projection_publication.h"

#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "mods.h"
#include "native_dispatch.h"

#include <string>

#include <cstdlib>
#include <lucent/log.h>

namespace crash {
namespace {

// $a0/$a1 carry the centre into set_geom_offset (the leaf shifts left by 16); set_geom_screen takes H in $a0.
constexpr int kCentreXArgument = 4;
constexpr int kCentreYArgument = 5;
constexpr int kScreenDistanceArgument = 4;

// A `jal` at A leaves `$r31 = A + 8` (after the delay slot), so the call site is `$r31 - 8`.
constexpr int kReturnAddressRegister = 31;
constexpr int kCallSiteBelowReturnAddress = 8;

// The leaf's `sll 16` truncates a 32-bit $a0; an unproducible centre is refused, not wrapped.
constexpr int kMaximumCentre = 0x7FFF;

} // namespace

GuestProjectionPublication::GuestProjectionPublication(const ProjectionTitleFacts &facts, Latch latch)
    : facts_(facts), latch_(latch) {
  if (!latch_) {
    lucent::error(facts_.serial, "guest widescreen requires the shared plan latch");
    std::abort();
  }
  if (facts_.projectionInit == 0 || facts_.setGeomOffset == 0 || facts_.setGeomScreen == 0) {
    lucent::error(facts_.serial,
                  "guest widescreen has no measured publication entry; refusing rather than guessing a "
                  "guest address");
    std::abort();
  }
  if (facts_.centreReach == CentreReach::ReturnAddressCallSites &&
      (facts_.centreCallSites == nullptr || facts_.centreCallSiteCount == 0)) {
    lucent::error(facts_.serial,
                  "guest widescreen declares the $r31 call-site recovery but names no call site; an "
                  "override reached from an unnamed site cannot be widened safely");
    std::abort();
  }
  if (facts_.centreReach == CentreReach::IndirectCall && facts_.passThroughCallSiteCount != 0) {
    lucent::error(facts_.serial,
                  "guest widescreen reaches its centre leaf indirectly, so $r31 cannot select a "
                  "pass-through site; a declared pass-through list here would be a list nothing can "
                  "honour");
    std::abort();
  }
}

bool GuestProjectionPublication::isGuestRam(std::uint32_t address) {
  // A null record is never valid, and physical address 0 is the BIOS/KSEG-aliased region.
  constexpr std::uint32_t kKseg0Base = 0x80000000u;
  constexpr std::uint32_t kKseg1Base = 0xA0000000u;
  constexpr std::uint32_t kMainRamBytes = 0x00200000u;
  constexpr std::uint32_t kParallelRamBytes = 0x00100000u;

  if (address == 0) {
    return false;
  }
  if (address >= kKseg0Base && address - kKseg0Base < kMainRamBytes) {
    return true;
  }
  if (address >= kKseg1Base && address - kKseg1Base < kParallelRamBytes) {
    return true;
  }
  return address < kMainRamBytes;
}

GuestProjectionGeometry GuestProjectionPublication::measuredGeometry(int displayWidth, int displayHeight) const {
  // The PSX default draw area is the whole display, so its horizontal extent is the display extent.
  if (displayWidth <= 0 || displayHeight <= 0) {
    lucent::error(facts_.serial,
                  "the guest published no usable display extent ({}x{}); the title's draw area is the "
                  "display area, so refusing is better than guessing a 4:3 width",
                  displayWidth,
                  displayHeight);
    std::abort();
  }
  return {{displayWidth, displayHeight}, displayWidth};
}

std::int32_t GuestProjectionPublication::widenedCentreX(std::int32_t retailCentreX, int margin) const {
  // `retail + margin`, never `read + margin`: the argument is the title's own value, so it cannot compound.
  const std::int64_t widened = static_cast<std::int64_t>(retailCentreX) + margin;
  if (widened < -kMaximumCentre || widened > kMaximumCentre) {
    lucent::error(facts_.serial,
                  "widening the guest centre {} by {} leaves the representable 16.16 range",
                  retailCentreX,
                  margin);
    std::abort();
  }
  return static_cast<std::int32_t>(widened);
}

bool GuestProjectionPublication::passesThrough(std::uint32_t callSite) const {
  for (std::size_t index = 0; index < facts_.passThroughCallSiteCount; ++index) {
    if (facts_.passThroughCallSites[index] == callSite) {
      return true;
    }
  }
  return false;
}

bool GuestProjectionPublication::knowsCallSite(std::uint32_t callSite) const {
  for (std::size_t index = 0; index < facts_.centreCallSiteCount; ++index) {
    if (facts_.centreCallSites[index] == callSite) {
      return true;
    }
  }
  return false;
}

std::uint32_t GuestProjectionPublication::observedCallSite(const Core &core, const char *site) const {
  const auto returnAddress = static_cast<std::uint32_t>(core.r[kReturnAddressRegister]);
  if (returnAddress < kCallSiteBelowReturnAddress) {
    lucent::error(facts_.serial, "{} override reached with $r31 = 0x{:08X}", site, returnAddress);
    std::abort();
  }
  const std::uint32_t callSite = returnAddress - kCallSiteBelowReturnAddress;
  if (knowsCallSite(callSite)) {
    return callSite;
  }
  // An unmeasured call site may already carry a widening, so it is refused rather than widened.
  lucent::error(facts_.serial,
                "{} override was reached from 0x{:08X}, which the title manifest does not name; this "
                "repository will not widen a call site whose argument provenance it has not measured",
                site,
                callSite);
  std::abort();
}

GuestProjectionPlan GuestProjectionPublication::relatch(Core &core) {
  GuestProjectionPlan latched = latch_(&core, measuredGeometry(core.game->gpu.s_disp_w, core.game->gpu.s_disp_h));
  if (latched.projectionCenterX <= 0 || latched.guestDrawWidth <= 0 || latched.projectionCenterX > kMaximumCentre) {
    lucent::error(facts_.serial,
                  "the framework returned an unusable guest projection (centre={}, draw width={})",
                  latched.projectionCenterX,
                  latched.guestDrawWidth);
    std::abort();
  }
  plan_ = latched;
  return latched;
}

void GuestProjectionPublication::publishCentre(Core &core, const RetailBody &retail) {
  if (!core.game) {
    lucent::error(facts_.serial, "the centre publication reached a Core with no Game");
    std::abort();
  }
  if (!retail) {
    lucent::error(facts_.serial, "the centre publication requires the retail guest body");
    std::abort();
  }
  // The call site is only recoverable for a `jal` reach; an indirect title widens every call.
  const bool widen =
      facts_.centreReach == CentreReach::IndirectCall || !passesThrough(observedCallSite(core, "set_geom_offset"));

  // The base is the argument register as it stands now, never a remembered value; whole pixels, since
  // the leaf shifts by 16.
  const auto retailX = static_cast<std::int32_t>(core.r[kCentreXArgument]);
  const auto retailY = static_cast<std::int32_t>(core.r[kCentreYArgument]);
  retail_ = {retailX, retailY};

  const GuestProjectionPlan latched = relatch(core);
  if (latched.widescreen() && widen) {
    core.r[kCentreXArgument] = static_cast<std::uint32_t>(widenedCentreX(retailX, latched.projectionHorizontalMargin));
  } else {
    // Zero margin: the title's argument reaches the leaf untouched. A pass-through site takes this arm too.
    core.r[kCentreXArgument] = static_cast<std::uint32_t>(retailX);
  }
  // The vertical centre is never moved; OFY and the vertical FOV stay fixed.
  core.r[kCentreYArgument] = static_cast<std::uint32_t>(retailY);

  // Capture the argument before the leaf runs: the retail leaf at 0x80042F8C is `sll $a0,$a0,0x10` and
  // shifts the register in place.
  const std::int32_t expectedX = static_cast<std::int32_t>(core.r[kCentreXArgument]);

  retail(core);

  // Signed 16.16: the int16_t round-trip keeps a camera-shake centre negative, where `>> 16` would not.
  const auto publishedX =
      static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(facts_.centreXRegister) >> 16));
  const auto publishedY =
      static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(facts_.centreYRegister) >> 16));
  if (publishedY != retail_.y) {
    lucent::error(facts_.serial,
                  "the guest published the vertical centre {} from an unchanged $a1 = {}; a vertical "
                  "shift moves the picture without widening it",
                  publishedY,
                  retail_.y);
    std::abort();
  }
  if (publishedX != expectedX) {
    lucent::error(facts_.serial,
                  "the guest published the horizontal centre {} but $a0 was {}; the plan and the "
                  "guest's own leaf disagree, so the frame would not be the widening it claims",
                  publishedX,
                  expectedX);
    std::abort();
  }
  if (latched.widescreen() && widen && retailX != 0) {
    lucent::info(facts_.serial,
                 "guest centre {} -> {} (retail {} + margin {}, vertical {}, H {}), host canvas {} "
                 "(native {})",
                 retailX,
                 publishedX,
                 retailX,
                 latched.projectionHorizontalMargin,
                 publishedY,
                 publishedScreenDistance_,
                 latched.presentationExtent.width,
                 latched.nativeExtent.width);
  }
  published_ = true;
  if (widen) {
    ++publications_;
  } else {
    ++passThroughs_;
  }
}

void GuestProjectionPublication::publishInitProjection(Core &core, const RetailBody &retail) {
  if (!core.game) {
    lucent::error(facts_.serial, "the projection init reached a Core with no Game");
    std::abort();
  }
  if (!retail) {
    lucent::error(facts_.serial, "the init projection publication requires the retail guest body");
    std::abort();
  }
  retail(core);

  // The init writes the registers inline and never passes through a leaf, so the baseline is read from
  // the coprocessor and compared against the manifest.
  publishedScreenDistance_ = static_cast<std::int32_t>(gte_read_ctrl(facts_.screenDistanceRegister) & 0xFFFF);
  const std::int32_t retailX =
      static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(facts_.centreXRegister) >> 16));
  const std::int32_t retailY =
      static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(facts_.centreYRegister) >> 16));
  if (publishedScreenDistance_ != facts_.retailScreenDistance || retailX != facts_.retailCentreX ||
      retailY != facts_.retailCentreY) {
    lucent::error(facts_.serial,
                  "the projection init published H {} centre {} {}; the manifest records H {} centre "
                  "{} {}",
                  publishedScreenDistance_,
                  retailX,
                  retailY,
                  facts_.retailScreenDistance,
                  facts_.retailCentreX,
                  facts_.retailCentreY);
    std::abort();
  }
  retail_ = {retailX, retailY};
  relatch(core);
  lucent::info(facts_.serial,
               "guest projection init published H {}, centre {} {} - the retail 4:3 baseline this owner "
               "widens; host canvas {} (native {})",
               publishedScreenDistance_,
               retailX,
               retailY,
               plan_.presentationExtent.width,
               plan_.nativeExtent.width);
}

void GuestProjectionPublication::observeScreenDistance(Core &core, const RetailBody &retail) {
  if (!core.game) {
    lucent::error(facts_.serial, "the screen-distance publication reached a Core with no Game");
    std::abort();
  }
  if (!retail) {
    lucent::error(facts_.serial, "the screen-distance publication requires the retail guest body");
    std::abort();
  }
  // H is held fixed: the retail body runs untouched and its published value is recorded, which makes
  // "the owner never touches H" checkable.
  retail(core);
  publishedScreenDistance_ = static_cast<std::int32_t>(core.r[kScreenDistanceArgument] & 0xFFFF);
}

namespace {

// Each override runs the retail body through the executor's original-call path.
void originalCentre(Core &core, const ProjectionTitleFacts &facts) {
  psx::cpu::callOriginalToReturn(
      core, facts.setGeomOffset, psx::cpu::ExecutionBudget::currentTurn(core), "guest widescreen::centre original");
}

void originalInitProjection(Core &core, const ProjectionTitleFacts &facts) {
  psx::cpu::callOriginalToReturn(core,
                                 facts.projectionInit,
                                 psx::cpu::ExecutionBudget::currentTurn(core),
                                 "guest widescreen::init projection original");
}

void originalScreenDistance(Core &core, const ProjectionTitleFacts &facts) {
  psx::cpu::callOriginalToReturn(core,
                                 facts.setGeomScreen,
                                 psx::cpu::ExecutionBudget::currentTurn(core),
                                 "guest widescreen::screen distance original");
}

void centreOverride(Core *core) {
  GuestProjectionPublication &owner = GuestProjectionPublication::from(*core, "set_geom_offset");
  owner.publishCentre(*core, [&owner](Core &target) {
    originalCentre(target, owner.facts());
  });
}

void initProjectionOverride(Core *core) {
  GuestProjectionPublication &owner = GuestProjectionPublication::from(*core, "projection init");
  owner.publishInitProjection(*core, [&owner](Core &target) {
    originalInitProjection(target, owner.facts());
  });
}

void screenDistanceOverride(Core *core) {
  GuestProjectionPublication &owner = GuestProjectionPublication::from(*core, "set_geom_screen");
  owner.observeScreenDistance(*core, [&owner](Core &target) {
    originalScreenDistance(target, owner.facts());
  });
}

} // namespace

GuestProjectionPublication &GuestProjectionPublication::from(Core &core, std::string_view site) {
  if (!core.runtime) {
    lucent::error("crash-wide", "the {} override ran without a title runtime", site);
    std::abort();
  }
  auto *const policy = dynamic_cast<GuestProjectionPublication *>(
      const_cast<GuestWidescreenProjection *>(core.runtime->guestWidescreenProjection()));
  if (!policy) {
    lucent::error("crash-wide", "the {} override reached another title's projection policy", site);
    std::abort();
  }
  return *policy;
}

const GuestProjectionPublication &GuestProjectionPublication::from(const Core &core, std::string_view site) {
  return const_cast<GuestProjectionPublication &>(from(const_cast<Core &>(core), site));
}

void GuestProjectionPublication::installSites(Core &core) {
  const struct Binding {
    std::uint32_t address;
    std::string name;
    psx::cpu::NativeFunction function;
  } bindings[]{
      {facts_.setGeomOffset, std::string(facts_.serial) + " SetGeomOffset", centreOverride},
      {facts_.projectionInit, std::string(facts_.serial) + " GTE projection init", initProjectionOverride},
      {facts_.setGeomScreen, std::string(facts_.serial) + " SetGeomScreen", screenDistanceOverride},
  };
  for (const Binding &binding : bindings) {
    psx::cpu::installNativeOverride(core, binding.address, binding.name, binding.function);
  }
  lucent::info(facts_.serial,
               "guest widescreen installed: SetGeomOffset 0x{:08X}, projection init 0x{:08X}, "
               "SetGeomScreen 0x{:08X}; retail centre {} {} and H {}",
               facts_.setGeomOffset,
               facts_.projectionInit,
               facts_.setGeomScreen,
               facts_.retailCentreX,
               facts_.retailCentreY,
               facts_.retailScreenDistance);
}

} // namespace crash
