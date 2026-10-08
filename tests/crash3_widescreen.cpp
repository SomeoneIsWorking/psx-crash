// Falsifiers for the guest-widescreen owner; the latch is the framework's `gpu_vk_latch_guest_projection`.
// Guest words (titles/crash3/executable.json):
//   0x8004F70C ctc2 $a0,0xC000 OFX    0x8004F710 ctc2 $a1,0xC800 OFY   (set_geom_offset 0x8004F704)
//   0x8004F724 ctc2 $a0,0xD000 H                                       (set_geom_screen)
//   0x8004F3C8 ctc2 $t0,0xD000 H=1000                                  (init 0x8004F37C)
//   0x8004F6E4 cfc2 $t0,$24 / 0x8004F6E8 cfc2 $t1,$25                  (FUN_8004f6e4, the read-back)
// 320 is the display extent measured from the guest's GP1(0xC0) publication.

#include "crash3_widescreen.h"

#include "core.h"
#include "crash3_runtime.h"
#include "game.h"
#include "game_runtime.h"
#include "gpu_vk.h"
#include "hw_bind.h"
#include "mods.h"
#include "native_dispatch.h"
#include "psx_exe_image.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using crash::GuestProjectionPublication;
using crash3::Crash3Widescreen;

// Register numbers come from the owner's facts: CR[24]=OFX, CR[25]=OFY, CR[26]=H.
const crash::ProjectionTitleFacts kFacts = Crash3Widescreen::facts();

// The guest's own leaf from 0x8004F704.
//   0x8004F704 sll $a0,16 / 0x8004F708 sll $a1,16
//   0x8004F70C ctc2 $a0,0xC000 (CR[24]=OFX) / 0x8004F710 ctc2 $a1,0xC800 (CR[25]=OFY)
void retailCentre(Core &core) {
  gte_write_ctrl(kFacts.centreXRegister, static_cast<std::uint32_t>(core.r[4]) << 16);
  gte_write_ctrl(kFacts.centreYRegister, static_cast<std::uint32_t>(core.r[5]) << 16);
}

// The guest's own projection init, 0x8004F37C, word for word.
void retailInitProjection(Core &core) {
  gte_write_ctrl(29, 0x155);
  gte_write_ctrl(30, 0x100);
  gte_write_ctrl(kFacts.screenDistanceRegister, 1000);
  gte_write_ctrl(27, 0xFFFFEF9Eu);
  gte_write_ctrl(28, 0x01400000u);
  gte_write_ctrl(kFacts.centreXRegister, 0);
  gte_write_ctrl(kFacts.centreYRegister, 0);
}

// The guest's own set_geom_screen, 0x8004F724: one `ctc2 $a0,0xD000`.
void retailScreenDistance(Core &core) {
  gte_write_ctrl(kFacts.screenDistanceRegister, core.r[4] & 0xFFFF);
}

// The guest's FUN_8004f6e4 read-back from 0x8004F6E4: `cfc2 $t0,$24 / cfc2 $t1,$25 / sra 16 / sra 16 /
// sw $t0,0($a0) / sw $t1,0($a1)`. It has the shape of the value the guest hands back, so widening it
// would change a guest descriptor.
void retailReadBack(Core &core, std::uint32_t centreX, std::uint32_t centreY) {
  gte_write_ctrl(kFacts.centreXRegister, centreX << 16);
  gte_write_ctrl(kFacts.centreYRegister, centreY << 16);
}

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
  }
  return condition;
}

std::vector<std::uint8_t> residentFixture() {
  constexpr std::uint32_t kTextAddress = 0x80010000u;
  constexpr std::uint32_t kTextSize = 0x51000u;
  std::vector<std::uint8_t> bytes(psx::cpu::kPsxExeHeaderBytes + kTextSize, 0u);
  const std::array<char, 8> kMagic{'P', 'S', '-', 'X', ' ', 'E', 'X', 'E'};
  for (std::size_t index = 0; index < kMagic.size(); ++index) {
    bytes[index] = static_cast<std::uint8_t>(kMagic[index]);
  }
  auto put = [&bytes](std::size_t at, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
      bytes[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
    }
  };
  put(0x10, kTextAddress);
  put(0x18, kTextAddress);
  put(0x1C, kTextSize);
  put(0x30, 0x801FFFF0u);
  const struct {
    std::uint32_t address;
    std::uint32_t word;
  } site_words[]{
      {0x8004F3C8u, 0x48C8D000u}, // ctc2 $t0,0xD000 -> CR[26] = 0x3E8
      {0x8004F3E8u, 0x48C0C000u}, // ctc2 $zero,0xC000 -> CR[24] = 0
      {0x8004F3ECu, 0x48C0C800u}, // ctc2 $zero,0xC800 -> CR[25] = 0
      {0x8004F70Cu, 0x48C4C000u}, // ctc2 $a0,0xC000 -> CR[24] = $a0
      {0x8004F710u, 0x48C5C800u}, // ctc2 $a1,0xC800 -> CR[25] = $a1
      {0x8004F724u, 0x48C4D000u}, // ctc2 $a0,0xD000 -> CR[26] = $a0
      {0x8004F6E4u, 0x4848C000u}, // cfc2 $t0,$24 -> the read-back of OFX
      {0x8004F6E8u, 0x4849C800u}, // cfc2 $t1,$25 -> the read-back of OFY
  };
  for (const auto &site : site_words) {
    put(psx::cpu::kPsxExeHeaderBytes + (site.address - kTextAddress), site.word);
  }
  return bytes;
}

bool installWithNoHlePlan() {
  bool ok = true;
  crash3::Crash3Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_init();
  gte_bind(&core);
  core.rsub.mode.setPath(RenderPath::Gte);
  core.game->gpu.s_disp_w = 320;
  core.game->gpu.s_disp_h = 240;
  core.game->mods.aspect = ASPECT_16_9;

  const auto bytes = residentFixture();
  const auto loaded = psx::cpu::loadPsxExeImage(core, bytes, "crash3-wide-install-fixture");
  ok &= expect(static_cast<bool>(loaded), "the install fixture did not establish a resident image");
  const std::uint32_t sites[] = {crash3::kProjectionInit, crash3::kSetGeomOffset, crash3::kSetGeomScreen};
  for (const std::uint32_t address : sites) {
    ok &= expect(core.game->platform_hle.lookup(address) == nullptr,
                 "a measured projection leaf was already in the HLE table, so the no-HLE proof is vacuous");
  }

  runtime.widescreen().installSites(core);
  for (const std::uint32_t address : sites) {
    const auto image = core.currentImageIdentity(address);
    ok &= expect(image.has_value(), "no image identity at an override address");
    if (!image) {
      continue;
    }
    ok &= expect(core.nativeDispatcher().isInstalled({*image, address}),
                 "a measured projection leaf did not install with no HLE plan in existence");
    ok &= expect(core.nativeDispatcher().intercepts({*image, address}),
                 "an installed projection leaf does not intercept a guest call at its address");
  }
  return ok;
}

} // namespace

int main() {
  // The shipping runtime's owner, reached the way the framework reaches it.
  crash3::Crash3Runtime runtime;
  if (!expect(runtime.guestWidescreenProjection() == &runtime.widescreen(),
              "the runtime's guest-widescreen policy is not the owner it hands the framework")) {
    return 1;
  }
  if (!expect(runtime.guestWidescreenProjection() != nullptr,
              "guestWidescreenProjection() is null again; Crash 3 owns no projection owner")) {
    return 1;
  }

  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_init();
  gte_bind(&core);
  core.rsub.mode.setPath(RenderPath::Gte);
  core.game->gpu.s_disp_w = 320;
  core.game->gpu.s_disp_h = 240;
  Crash3Widescreen &owner = runtime.widescreen();
  bool ok = true;

  // The registers are signed 16.16; decode independently of the owner's expression.
  auto publishedOfx = [&owner] {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(owner.facts().centreXRegister) >> 16));
  };
  auto publishedOfy = [&owner] {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(owner.facts().centreYRegister) >> 16));
  };
  auto publishFrom = [&core, &owner](std::uint32_t callSite, std::int32_t x, std::int32_t y) {
    core.r[4] = static_cast<std::uint32_t>(x);
    core.r[5] = static_cast<std::uint32_t>(y);
    core.r[31] = callSite + 8; // a `jal` at `callSite` links `$r31` to its own address + 8
    owner.publishCentre(core, retailCentre);
  };
  auto publish = [&publishFrom](std::int32_t x, std::int32_t y) {
    publishFrom(crash3::kCentreCallSites[1], x, y); // the per-frame site, 0x80018C04
  };

  // 1. 4:3 identity at every measured call site, pass-through included: zero margin leaves the argument untouched.
  core.game->mods.aspect = ASPECT_4_3;
  for (const std::uint32_t site : crash3::kCentreCallSites) {
    publishFrom(site, 13, -7);
    ok &= expect(publishedOfx() == 13 && publishedOfy() == -7 && core.r[4] == 13 && core.r[5] == -7,
                 "4:3 identity changed a non-zero retail centre at a measured call site");
  }
  ok &= expect(!owner.plan().widescreen(), "4:3 produced a widescreen plan");
  ok &= expect(owner.plan().projectionHorizontalMargin == 0, "4:3 produced a non-zero horizontal margin");

  // 2. Widening: 16:9 on 320 is 428, margin (428-320)/2 = 54; OFY and H untouched.
  core.game->mods.aspect = ASPECT_16_9;
  publish(0, 0);
  ok &= expect(owner.plan().widescreen(), "16:9 did not produce a widescreen plan");
  ok &= expect(owner.plan().presentationExtent.width == 428 && owner.plan().nativeExtent.width == 320,
               "the 16:9 presentation extent is not 428 over a 320 native");
  ok &= expect(owner.plan().projectionHorizontalMargin == 54, "the 16:9 horizontal margin is not 54");
  ok &= expect(publishedOfx() == 54, "the widened guest OFX is not 54");
  ok &= expect(publishedOfy() == 0 && core.r[5] == 0,
               "the widening moved OFY, which is a vertical shift and not a widening");
  ok &= expect(owner.published(), "a widened publication did not record that it published");

  // 3. Idempotence at the widening sites: 54 stays 54, not 108.
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "a second publication compounded the margin instead of reusing it");
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "a third publication compounded the margin");

  // 4. Pass-through. FUN_8004f6e4 reads the centre back and FUN_8001cd80 hands it to set_geom_offset at
  //    0x8001D09C, so the argument already carries the margin and `retail + margin` would add it each pass.
  //    The mutant is a policy with an empty pass-through list, which case 5 shows widens that site.
  ok &= expect(owner.declaresPassThrough(),
               "Crash 3 declares no pass-through call site although the census found a read-back");
  ok &= expect(owner.passesThrough(crash3::kPassThroughCallSites[0]),
               "the read-back republication site is not on the pass-through list");
  ok &= expect(crash3::kPassThroughCallSites[0] == 0x8001D09Cu,
               "the pass-through call site is not the measured read-back republication site");
  for (std::size_t index = 0; index < crash3::kCentreCallSiteCount; ++index) {
    const std::uint32_t site = crash3::kCentreCallSites[index];
    ok &= expect(owner.passesThrough(site) == (site == crash3::kPassThroughCallSites[0]),
                 "a measured call site is on the wrong side of the pass-through policy");
  }

  // 5. The pass-through site leaves a widened centre alone and every other site widens it; the value fed
  //    is what the read-back would hand over.
  const std::int32_t readBackValue = 54; // what CR[24] holds after a widened publication
  retailReadBack(core, static_cast<std::uint32_t>(readBackValue), 0u);
  const std::size_t wideningsBeforePassThrough = owner.publications();
  const std::size_t passThroughsBefore = owner.passThroughs();
  publishFrom(crash3::kPassThroughCallSites[0], readBackValue, 0);
  ok &= expect(publishedOfx() == readBackValue,
               "the pass-through site changed a guest descriptor the guest had already widened");
  ok &= expect(owner.passThroughs() == passThroughsBefore + 1 && owner.publications() == wideningsBeforePassThrough,
               "the pass-through publication was counted as a widening, or not counted as a pass-through");

  for (const std::uint32_t site :
       {crash3::kCentreCallSites[0], crash3::kCentreCallSites[1], crash3::kCentreCallSites[2]}) {
    publishFrom(site, 0, 0);
    ok &= expect(publishedOfx() == 54, "a widening call site did not widen the centre");
  }
  ok &= expect(owner.publications() == wideningsBeforePassThrough + 3 && owner.passThroughs() == passThroughsBefore + 1,
               "three widening sites and one pass-through site did not report three widenings and one "
               "pass-through");

  // 6. An unmeasured call site is refused, not widened by default. `publishCentre` aborts on one, so the
  //    predicate is asserted.
  ok &= expect(!owner.knowsCallSite(0x80000000u),
               "a call site the manifest does not name is treated as one this owner knows");
  ok &= expect(!owner.passesThrough(0x80000000u), "a call site the manifest does not name is on the pass-through list");
  for (const std::uint32_t site : crash3::kCentreCallSites) {
    ok &= expect(owner.knowsCallSite(site), "a measured centre call site is not one this owner knows");
  }

  // 7. 0x80018C04 publishes SetGeomOffset(DAT_80068FA8 + 0x100, ...), a live camera global; the widening rides it.
  publish(-5, 3);
  ok &= expect(publishedOfx() == 49 && publishedOfy() == 3,
               "the widening did not ride a non-zero retail centre, or it moved the vertical one");

  // 8. Unwidening: the next publication carries the retail centre again.
  core.game->mods.aspect = ASPECT_4_3;
  publish(0, 0);
  ok &= expect(publishedOfx() == 0, "returning to 4:3 left the widened centre published");
  core.game->mods.aspect = ASPECT_16_9;

  // 9. H is never touched: 0x8004F724 is overridden; widening H would also move the near plane at
  //    0x8003C3D0 and the 2D overlay at 0x80016634.
  publish(0, 0);
  const std::int32_t widenedBefore = publishedOfx();
  core.r[4] = 288; // the camera setup's own H in FUN_800188ec
  owner.observeScreenDistance(core, retailScreenDistance);
  ok &= expect(static_cast<std::int32_t>(gte_read_ctrl(owner.facts().screenDistanceRegister)) == 288,
               "the screen-distance site did not let the guest publish its own H");
  ok &= expect(owner.publishedScreenDistance() == 288, "the published H was not recorded");
  core.r[4] = 1000; // the projection init's value
  owner.observeScreenDistance(core, retailScreenDistance);
  ok &= expect(static_cast<std::int32_t>(gte_read_ctrl(owner.facts().screenDistanceRegister)) == 1000,
               "a second H publication was not let through");
  ok &= expect(publishedOfx() == widenedBefore && publishedOfx() == 54,
               "the screen-distance site changed the horizontal centre; H is the scale, OFX is the field");

  // 10. The retail baseline is measured.
  Crash3Widescreen fresh{Crash3Widescreen::facts(), &gpu_vk_latch_guest_projection};
  retailInitProjection(core);
  fresh.publishInitProjection(core, retailInitProjection);
  ok &= expect(fresh.retailCentre().x == 0 && fresh.retailCentre().y == 0,
               "the init publication's centre was not read back from the guest's own registers");
  ok &= expect(fresh.publishedScreenDistance() == 1000, "the init publication's H was not recorded");
  ok &= expect(fresh.plan().presentationExtent.width == 428,
               "the init site did not latch the plan for the live display extent");
  ok &= expect(fresh.plan().projectionHorizontalMargin == 54, "the init site latched the wrong margin");

  // 11. No 4:3 width is baked in: a title that published 368 widens 368 -> 492.
  core.game->gpu.s_disp_w = 368;
  publish(0, 0);
  ok &= expect(owner.plan().nativeExtent.width == 368 && owner.plan().presentationExtent.width == 492 &&
                   owner.plan().projectionHorizontalMargin == 62,
               "the owner did not measure the title's own display extent");
  core.game->gpu.s_disp_w = 320;

  // 12. Guest RAM bound.
  ok &= expect(GuestProjectionPublication::isGuestRam(crash3::kScreenDistanceCache) &&
                   GuestProjectionPublication::isGuestRam(0x80100000u) && !GuestProjectionPublication::isGuestRam(0) &&
                   !GuestProjectionPublication::isGuestRam(0x80200000u),
               "the guest RAM bound accepted an address outside the 2 MiB main RAM");

  ok &= installWithNoHlePlan();

  if (!ok) {
    return 1;
  }
  std::printf("Crash 3 guest widescreen: 4:3 identity exact at all four measured call sites, 16:9 "
              "moves the centre at three of them, the read-back site at 0x%08X passes through so the "
              "margin cannot compound, and H is held because it is this title's near plane and a HUD "
              "scalar\n",
              crash3::kPassThroughCallSites[0]);
  return 0;
}
