// Falsifiers for the guest-widescreen owner; the latch is the framework's `gpu_vk_latch_guest_projection`.
// Guest words (titles/crash2/executable.json):
//   0x8004EFF0 ctc2 $a0,0xC000 OFX    0x8004EFF4 ctc2 $a1,0xC800 OFY   (set_geom_offset 0x8004EFE8)
//   0x8004F008 ctc2 $a0,0xD000 H                                       (set_geom_screen)
//   0x8004EC7C ctc2 $t0,0xD000 H=1000, 0x8004EC9C ctc2 $zero,0xC000 OFX=0   (init 0x8004EC30)
// 320 is the display extent measured from the guest's GP1(0xC0) publication.

#include "crash2_widescreen.h"

#include "core.h"
#include "crash2_runtime.h"
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
using crash2::Crash2Widescreen;

// Register numbers come from the owner's facts: CR[24]=OFX, CR[25]=OFY, CR[26]=H.
const crash::ProjectionTitleFacts kFacts = Crash2Widescreen::facts();

// The guest's own leaf from 0x8004EFE8, recomputed so a test tells the owner writing the centre from the
// leaf producing it.
//   0x8004EFE8 sll $a0,16 / 0x8004EFEC sll $a1,16
//   0x8004EFF0 ctc2 $a0,0xC000 (CR[24]=OFX) / 0x8004EFF4 ctc2 $a1,0xC800 (CR[25]=OFY)
void retailCentre(Core &core) {
  gte_write_ctrl(kFacts.centreXRegister, static_cast<std::uint32_t>(core.r[4]) << 16);
  gte_write_ctrl(kFacts.centreYRegister, static_cast<std::uint32_t>(core.r[5]) << 16);
}

// The guest's projection init 0x8004EC30: 0x8004EC64 CR[29]=0x155, 0x8004EC70 CR[30]=0x100, 0x8004EC7C
// CR[26]=0x3E8, 0x8004EC88 CR[27]=0xEF9E, 0x8004EC94 CR[28]=0x01400000, 0x8004EC9C CR[24]=0, 0x8004ECA0
// CR[25]=0. The last two are `ctc2 $zero`, so only driving this site reaches them.
void retailInitProjection(Core &core) {
  gte_write_ctrl(29, 0x155);
  gte_write_ctrl(30, 0x100);
  gte_write_ctrl(kFacts.screenDistanceRegister, 1000);
  gte_write_ctrl(27, 0xFFFFEF9Eu);
  gte_write_ctrl(28, 0x01400000u);
  gte_write_ctrl(kFacts.centreXRegister, 0);
  gte_write_ctrl(kFacts.centreYRegister, 0);
}

// The guest's own set_geom_screen, 0x8004F008: one `ctc2 $a0,0xD000`.
void retailScreenDistance(Core &core) {
  gte_write_ctrl(kFacts.screenDistanceRegister, core.r[4] & 0xFFFF);
}

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
  }
  return condition;
}

// A synthetic PS-X EXE spanning the resident text with the real instruction words at their real offsets.
std::vector<std::uint8_t> residentFixture() {
  constexpr std::uint32_t kTextAddress = 0x80010000u;
  constexpr std::uint32_t kTextSize = 0x4F800u;
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
  put(0x10, kTextAddress); // pc0
  put(0x18, kTextAddress); // text address
  put(0x1C, kTextSize);    // text size
  put(0x30, 0x801FFFF0u);  // stack address
  const struct {
    std::uint32_t address;
    std::uint32_t word;
  } site_words[]{
      // 0x8004EC7C ctc2 $t0,0xD000  -> CR[26] = 0x3E8
      {0x8004EC7Cu, 0x48C8D000u},
      // 0x8004EC9C ctc2 $zero,0xC000 -> CR[24] = 0
      {0x8004EC9Cu, 0x48C0C000u},
      // 0x8004ECA0 ctc2 $zero,0xC800 -> CR[25] = 0
      {0x8004ECA0u, 0x48C0C800u},
      // 0x8004EFF0 ctc2 $a0,0xC000  -> CR[24] = $a0
      {0x8004EFF0u, 0x48C4C000u},
      // 0x8004EFF4 ctc2 $a1,0xC800  -> CR[25] = $a1
      {0x8004EFF4u, 0x48C5C800u},
      // 0x8004F008 ctc2 $a0,0xD000  -> CR[26] = $a0
      {0x8004F008u, 0x48C4D000u},
  };
  for (const auto &site : site_words) {
    put(psx::cpu::kPsxExeHeaderBytes + (site.address - kTextAddress), site.word);
  }
  return bytes;
}

// Install proof with no HLE plan: PlatformHle does not cover these leaves.
bool installWithNoHlePlan() {
  bool ok = true;
  crash2::Crash2Runtime runtime;
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
  const auto loaded = psx::cpu::loadPsxExeImage(core, bytes, "crash2-wide-install-fixture");
  ok &= expect(static_cast<bool>(loaded), "the install fixture did not establish a resident image");
  const std::uint32_t sites[] = {crash2::kProjectionInit, crash2::kSetGeomOffset, crash2::kSetGeomScreen};
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
    // `intercepts` is what routes a guest call.
    ok &= expect(core.nativeDispatcher().intercepts({*image, address}),
                 "an installed projection leaf does not intercept a guest call at its address");
  }
  return ok;
}

} // namespace

int main() {
  // The shipping runtime's owner, reached the way the framework reaches it.
  crash2::Crash2Runtime runtime;
  if (!expect(runtime.guestWidescreenProjection() == &runtime.widescreen(),
              "the runtime's guest-widescreen policy is not the owner it hands the framework")) {
    return 1;
  }
  if (!expect(runtime.guestWidescreenProjection() != nullptr,
              "guestWidescreenProjection() is null again; Crash 2 owns no projection owner")) {
    return 1;
  }

  // The framework binds a Core's GTE register file once per frame-step (hw_bind.h); bind as the product does.
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_init();
  gte_bind(&core);
  core.rsub.mode.setPath(RenderPath::Gte);
  // The draw area is the whole display area, so the one horizontal extent is the one published through GP1 0xC0.
  core.game->gpu.s_disp_w = 320;
  core.game->gpu.s_disp_h = 240;
  Crash2Widescreen &owner = runtime.widescreen();
  bool ok = true;

  // The registers are signed 16.16; decode independently of the owner's expression.
  auto publishedOfx = [&owner] {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(owner.facts().centreXRegister) >> 16));
  };
  auto publishedOfy = [&owner] {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(gte_read_ctrl(owner.facts().centreYRegister) >> 16));
  };
  // The owner recovers the call site from `$r31 - 8`, so the driver sets it.
  auto publishFrom = [&core, &owner](std::uint32_t callSite, std::int32_t x, std::int32_t y) {
    core.r[4] = static_cast<std::uint32_t>(x);
    core.r[5] = static_cast<std::uint32_t>(y);
    core.r[31] = callSite + 8; // a `jal` at `callSite` links `$r31` to its own address + 8
    owner.publishCentre(core, retailCentre);
  };
  auto publish = [&publishFrom](std::int32_t x, std::int32_t y) {
    publishFrom(crash2::kCentreCallSites[1], x, y); // the per-frame site, 0x80017F70
  };

  // 1. 4:3 identity: zero margin, so the leaf produces retail's CR[24] bit for bit.
  core.game->mods.aspect = ASPECT_4_3;
  publish(0, 0);
  ok &= expect(publishedOfx() == 0 && publishedOfy() == 0 && core.r[4] == 0 && core.r[5] == 0,
               "4:3 identity: the retail centre did not survive the publication byte for byte");
  ok &= expect(!owner.plan().widescreen(), "4:3 produced a widescreen plan");
  ok &= expect(owner.plan().projectionHorizontalMargin == 0, "4:3 produced a non-zero horizontal margin");

  // 2. Publish a non-zero centre, so an owner that always added the margin is caught.
  publish(7, 11);
  ok &= expect(publishedOfx() == 7 && publishedOfy() == 11 && core.r[4] == 7 && core.r[5] == 11,
               "4:3 identity changed a non-zero retail centre");

  // 3. Widening: 16:9 on 320 is 428, margin (428-320)/2 = 54; OFY and H untouched.
  core.game->mods.aspect = ASPECT_16_9;
  publish(0, 0);
  ok &= expect(owner.plan().widescreen(), "16:9 did not produce a widescreen plan");
  ok &= expect(owner.plan().presentationExtent.width == 428 && owner.plan().nativeExtent.width == 320,
               "the 16:9 presentation extent is not 428 over a 320 native");
  ok &= expect(owner.plan().projectionHorizontalMargin == 54, "the 16:9 horizontal margin is not 54");
  ok &= expect(owner.plan().projectionCenterX == 214, "the 16:9 projection centre is not 214");
  ok &= expect(publishedOfx() == 54, "the widened guest OFX is not 54");
  ok &= expect(publishedOfy() == 0 && core.r[5] == 0,
               "the widening moved OFY, which is a vertical shift and not a widening");
  ok &= expect(owner.retailCentre().x == 0 && owner.retailCentre().y == 0,
               "the centre the guest published was not recorded as its own argument");
  ok &= expect(owner.published(), "a widened publication did not record that it published");
  // The counters are cumulative, so pin the delta one publication makes.
  const std::size_t widenedSoFar = owner.publications();
  const std::size_t passedThroughSoFar = owner.passThroughs();
  publish(0, 0);
  ok &= expect(owner.publications() == widenedSoFar + 1 && owner.passThroughs() == passedThroughSoFar,
               "a widening publication was not counted as one widening and no pass-through");

  // 4. The 4:3 frame spans [0,320) at retail and [54,374) widened, with no rescale.
  ok &= expect(owner.plan().presentationHorizontalMargin == (owner.plan().presentationExtent.width - 320) / 2,
               "the widened 4:3 frame is not translated into the wide canvas at its original scale");

  // 5. Idempotence: 54 stays 54, not 108; no cfc2 reads CR[24]/CR[25], so nothing feeds the margin back.
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "a second publication compounded the margin instead of reusing it");
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "a third publication compounded the margin");

  // 6. 0x80017F70 publishes SetGeomOffset(DAT_8006CC20 + 0x100, ...), a live camera global; the widening
  //    rides it.
  publish(-5, 0);
  ok &= expect(publishedOfx() == 49, "the widening did not ride a non-zero retail centre");

  // 7. Unwidening: the next publication carries the retail centre again.
  core.game->mods.aspect = ASPECT_4_3;
  publish(0, 0);
  ok &= expect(publishedOfx() == 0, "returning to 4:3 left the widened centre published");
  core.game->mods.aspect = ASPECT_16_9;

  // 8. Both measured call sites widen and the pass-through list is empty; a mutation inventing a
  //    pass-through would stop widening one site.
  ok &= expect(!owner.declaresPassThrough(),
               "Crash 2 declares a pass-through call site although the census found no read-back");
  for (const std::uint32_t site : crash2::kCentreCallSites) {
    ok &= expect(owner.knowsCallSite(site) && !owner.passesThrough(site),
                 "a measured centre call site is not known, or is on the pass-through list");
  }
  // An unnamed call site is refused; `publishCentre` aborts, so the predicate is asserted.
  ok &= expect(!owner.knowsCallSite(0x80000000u),
               "a call site the manifest does not name is treated as one this owner knows");
  publishFrom(crash2::kCentreCallSites[0], 0, 0); // the camera setup
  ok &= expect(publishedOfx() == 54, "the camera-setup call site did not widen");

  // 9. H is never touched: 0x8004F008 is overridden and the recorded H is the discriminator; widening H
  //    would also move the near plane at 0x8003D3D0 and the HUD rectangle at 0x8001645C.
  core.game->mods.aspect = ASPECT_16_9;
  publish(0, 0);
  const std::int32_t widenedBefore = publishedOfx();
  core.r[4] = 288; // the camera setup's own H in FUN_8001798c
  owner.observeScreenDistance(core, retailScreenDistance);
  ok &= expect(static_cast<std::int32_t>(gte_read_ctrl(owner.facts().screenDistanceRegister)) == 288,
               "the screen-distance site did not let the guest publish its own H");
  ok &= expect(owner.publishedScreenDistance() == 288, "the published H was not recorded");
  core.r[4] = 1000; // gte_init's value
  owner.observeScreenDistance(core, retailScreenDistance);
  ok &= expect(static_cast<std::int32_t>(gte_read_ctrl(owner.facts().screenDistanceRegister)) == 1000,
               "a second H publication was not let through");
  ok &= expect(publishedOfx() == widenedBefore && publishedOfx() == 54,
               "the screen-distance site changed the horizontal centre; H is the scale, OFX is the field");

  // 10. The retail baseline is measured: H = 1000 and the centre 0,0 from `$zero`.
  Crash2Widescreen fresh{Crash2Widescreen::facts(), &gpu_vk_latch_guest_projection};
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

  // 12. Guest RAM bound: a fixture of low addresses cannot catch a bound that ignores KSEG0.
  ok &= expect(GuestProjectionPublication::isGuestRam(crash2::kScreenDistanceCache) &&
                   GuestProjectionPublication::isGuestRam(0x80100000u) && !GuestProjectionPublication::isGuestRam(0) &&
                   !GuestProjectionPublication::isGuestRam(0x80200000u),
               "the guest RAM bound accepted an address outside the 2 MiB main RAM");

  // 13. ASPECT_AUTO resolves to 4:3 headless with no wide sink.
  core.game->mods.aspect = ASPECT_AUTO;
  ok &= expect(owner.presentationAspect(core) == PresentationAspect::MatchSink,
               "ASPECT_AUTO is not handed to the plan builder as MatchSink");
  core.game->mods.aspect = ASPECT_21_9;
  ok &= expect(owner.presentationAspect(core) == PresentationAspect::UltraWide21x9,
               "21:9 is not reported as UltraWide21x9");
  core.game->mods.aspect = ASPECT_16_9;

  ok &= installWithNoHlePlan();

  if (!ok) {
    return 1;
  }
  std::printf("Crash 2 guest widescreen: 4:3 identity exact, 16:9 moves the guest centre by the margin "
              "with OFY and H untouched, the margin cannot compound at either measured call site, and "
              "the init publication is read from the guest\n");
  return 0;
}
