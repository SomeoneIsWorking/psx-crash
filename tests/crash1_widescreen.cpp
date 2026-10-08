// Falsifiers for the guest-widescreen owner. The rule is the shared `crash::GuestProjectionPublication`
// and the latch is the framework's `gpu_vk_latch_guest_projection`; nothing here reimplements them.
// Guest words (titles/crash1/executable.json):
//   0x80042F94 ctc2 $a0,0xC000 OFX    0x80042F98 ctc2 $a1,0xC800 OFY   (set_geom_offset 0x80042F8C)
//   0x80042FAC ctc2 $a0,0xD000 H                                       (set_geom_screen)
//   0x80042B68 ctc2 $t0,0xD000 H=1000, 0x80042B88 ctc2 $zero,0xC000 OFX=0   (gte_init 0x80042B1C)
// 320 is the display extent measured from the guest's GP1(0xC0) publication (FUN_80041C38, GPU driver
// table entry 7 at 0x80054A40).

#include "crash1_widescreen.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "game_runtime.h"
#include "gpu_vk.h"
#include "hw_bind.h"
#include "mods.h"
#include "native_dispatch.h"
#include "psx_exe_image.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

using crash1::Crash1Widescreen;

// The guest's own leaf from 0x80042F8C, recomputed so a test tells the owner writing the centre from the
// leaf producing it.
//   0x80042F8C sll $a0,16 / 0x80042F90 sll $a1,16
//   0x80042F94 ctc2 $a0,0xC000 (CR[24]=OFX) / 0x80042F98 ctc2 $a1,0xC800 (CR[25]=OFY)
constexpr std::uint32_t kGteCrOfx = 24;
constexpr std::uint32_t kGteCrOfy = 25;
constexpr std::uint32_t kGteCrH = 26;

void retailCentre(Core &core) {
  gte_write_ctrl(kGteCrOfx, static_cast<std::uint32_t>(core.r[4]) << 16);
  gte_write_ctrl(kGteCrOfy, static_cast<std::uint32_t>(core.r[5]) << 16);
}

// The guest's gte_init 0x80042B1C: 0x80042B50 CR[29]=0x155, 0x80042B5C CR[30]=0x100, 0x80042B68 CR[26]=0x3E8,
// 0x80042B74 CR[27]=0xEF9E, 0x80042B80 CR[28]=0x01400000, 0x80042B88 CR[24]=0, 0x80042B8C CR[25]=0. The last
// two are `ctc2 $zero`, so only driving this site reaches them.
void retailInitProjection(Core &core) {
  core.r[8] = 0x155;
  gte_write_ctrl(29, 0x155);
  gte_write_ctrl(30, 0x100);
  core.r[8] = 1000;
  gte_write_ctrl(kGteCrH, 1000);
  gte_write_ctrl(27, 0xFFFFEF9Eu);
  gte_write_ctrl(28, 0x01400000u);
  gte_write_ctrl(kGteCrOfx, 0);
  gte_write_ctrl(kGteCrOfy, 0);
}

// The guest's own set_geom_screen, 0x80042FAC: one `ctc2 $a0,0xD000`.
void retailScreenDistance(Core &core) {
  gte_write_ctrl(kGteCrH, core.r[4] & 0xFFFF);
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
  constexpr std::uint32_t kTextSize = 0x46800u;
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
      // 0x80042B68 ctc2 $t0,0xD000  -> CR[26] = 0x3E8
      {0x80042B68u, 0x48C8D000u},
      // 0x80042B88 ctc2 $zero,0xC000 -> CR[24] = 0
      {0x80042B88u, 0x48C0C000u},
      // 0x80042B8C ctc2 $zero,0xC800 -> CR[25] = 0
      {0x80042B8Cu, 0x48C0C800u},
      // 0x80042F94 ctc2 $a0,0xC000  -> CR[24] = $a0
      {0x80042F94u, 0x48C4C000u},
      // 0x80042F98 ctc2 $a1,0xC800  -> CR[25] = $a1
      {0x80042F98u, 0x48C5C800u},
      // 0x80042FAC ctc2 $a0,0xD000  -> CR[26] = $a0
      {0x80042FACu, 0x48C4D000u},
  };
  for (const auto &site : site_words) {
    put(psx::cpu::kPsxExeHeaderBytes + (site.address - kTextAddress), site.word);
  }
  return bytes;
}

// Install proof with no HLE plan: PlatformHle does not cover these leaves, so installing on a Core that
// never initialised one shows the proof does not depend on `resolveHostDispatch` ordering.
bool installWithNoHlePlan() {
  bool ok = true;
  crash1::Crash1Runtime runtime;
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
  const auto loaded = psx::cpu::loadPsxExeImage(core, bytes, "crash1-wide-install-fixture");
  const std::uint32_t sites[] = {crash1::kProjectionInit, crash1::kSetGeomOffset, crash1::kSetGeomScreen};
  ok &= expect(static_cast<bool>(loaded), "the install fixture did not establish a resident image");
  // The stock library table has no entry for any of the three leaves.
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
    // `intercepts` is what routes a guest call, so this shows the leaf is reached through the override
    // table; the transformation itself is pinned case by case above.
    ok &= expect(core.nativeDispatcher().intercepts({*image, address}),
                 "an installed projection leaf does not intercept a guest call at its address");
  }

  // The widened centre comes out of the owner through the production entry, on this second Core.
  const auto image = core.currentImageIdentity(crash1::kSetGeomOffset);
  if (image) {
    ok &= expect(core.nativeDispatcher().isInstalled({*image, crash1::kSetGeomOffset}),
                 "SetGeomOffset is not installed on the install-proof Core");
  }
  return ok;
}

} // namespace

int main() {
  // The shipping runtime's owner, reached the way the framework reaches it.
  crash1::Crash1Runtime runtime;
  if (!expect(runtime.guestWidescreenProjection() == &runtime.widescreen(),
              "the runtime's widescreen policy is not the owner it hands the framework")) {
    return 1;
  }
  if (!expect(runtime.renderCapabilities().defaultPath == RenderPath::Record &&
                  !runtime.renderCapabilities().nativeRenderPath && runtime.renderCapabilities().temporalInterpolation,
              "Crash 1 no longer ships the Record path with no native producer and interpolation on")) {
    return 1;
  }

  // The latch ignores the requested aspect without a render path and a title policy, so wire both:
  // RenderPath::Gte is this title's shipping path. The runtime goes in before the Game, which captures it.
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The framework binds a Core's GTE register file once per frame-step (hw_bind.h); bind as the product does.
  gte_init();
  gte_bind(&core);
  core.rsub.mode.setPath(RenderPath::Gte);
  // The draw area is the PSX default whole-display area (FUN_80042A04 stores `(GPUSTAT & 0x3FFF) |
  // 0xE1001000` at 0x80042A58), so the one horizontal extent is the one published through GP1 0xC0.
  core.game->gpu.s_disp_w = 320;
  core.game->gpu.s_disp_h = 240;
  Crash1Widescreen &owner = runtime.widescreen();
  bool ok = true;

  auto publishedOfx = [&core] {
    return static_cast<std::int32_t>(gte_read_ctrl(kGteCrOfx) >> 16);
  };
  auto publishedOfy = [&core] {
    return static_cast<std::int32_t>(gte_read_ctrl(kGteCrOfy) >> 16);
  };
  auto publish = [&core, &owner](std::int32_t x, std::int32_t y) {
    // The owner recovers the call site from `$r31`: a `jal` at the site links the site plus 8.
    core.r[4] = static_cast<std::uint32_t>(x);
    core.r[5] = static_cast<std::uint32_t>(y);
    core.r[31] = crash1::kCentreCallSites[1] + 8;
    owner.publishCentre(core, retailCentre);
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

  // 4. The 4:3 frame spans [0,320) at retail and [54,374) widened, with no rescale
  //    (external/psxport/docs/presentation-contract.md, "What counts as a widening").
  ok &= expect(owner.plan().presentationHorizontalMargin == (owner.plan().presentationExtent.width - 320) / 2,
               "the widened 4:3 frame is not translated into the wide canvas at its original scale");

  // 5. Idempotence: the argument is the title's own value, so 54 stays 54, not 108.
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "a second publication compounded the margin instead of reusing it");
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "a third publication compounded the margin");
  // 5b. Reach is two `jal`s (0x8001783C camera setup, 0x80017F00 per-frame matrix update); `$r31 - 8`
  //     names both.
  ok &= expect(crash1::kCentreReach == crash::CentreReach::ReturnAddressCallSites && !owner.declaresPassThrough() &&
                   crash1::kCentreCallSites[0] == 0x8001783Cu && crash1::kCentreCallSites[1] == 0x80017F00u,
               "Crash 1 no longer states its two measured `jal` sites with no pass-through");
  ok &= expect(owner.knowsCallSite(crash1::kCentreCallSites[0]) && owner.knowsCallSite(crash1::kCentreCallSites[1]) &&
                   !owner.knowsCallSite(crash1::kCentreCallSites[0] + 4),
               "the owner does not recognise exactly its two measured call sites, and the delay slot "
               "of a `jal` must NOT be one of them");
  core.r[31] = crash1::kCentreCallSites[1] + 8; // what the per-frame `jal` at 0x80017F00 links
  publish(0, 0);
  ok &= expect(publishedOfx() == 54, "the per-frame call site did not widen");

  // 6. 0x80017F00 publishes SetGeomOffset(DAT_8006193C, ...), a live camera-shake global; the widening
  //    rides it.
  publish(-5, 0);
  ok &= expect(publishedOfx() == 49, "the widening did not ride a non-zero retail centre");

  // 7. Unwidening: the next publication carries the retail centre again.
  core.game->mods.aspect = ASPECT_4_3;
  publish(0, 0);
  ok &= expect(publishedOfx() == 0, "returning to 4:3 left the widened centre published");
  core.game->mods.aspect = ASPECT_16_9;

  // 8. H is never touched: 0x80042FAC is overridden and the recorded H is the discriminator against a
  //    widening done through H.
  core.game->mods.aspect = ASPECT_16_9;
  publish(0, 0);
  const std::int32_t widenedBefore = publishedOfx();
  core.r[4] = 288; // the 0x5A camera mode in FUN_80017790
  owner.observeScreenDistance(core, retailScreenDistance);
  ok &= expect(static_cast<std::int32_t>(gte_read_ctrl(kGteCrH)) == 288,
               "the screen-distance site did not let the guest publish its own H");
  ok &= expect(owner.publishedScreenDistance() == 288, "the published H was not recorded");
  core.r[4] = 500; // the 0x25 camera mode
  owner.observeScreenDistance(core, retailScreenDistance);
  ok &= expect(static_cast<std::int32_t>(gte_read_ctrl(kGteCrH)) == 500,
               "the per-camera-mode H was not republished by the guest");
  ok &= expect(publishedOfx() == widenedBefore && publishedOfx() == 54,
               "the screen-distance site changed the horizontal centre; H is the scale, OFX is the field");

  // 9. The retail baseline is measured: gte_init publishes H = 1000 and OFX = OFY = 0 from `$zero`.
  Crash1Widescreen fresh{Crash1Widescreen::facts(), gpu_vk_latch_guest_projection};
  retailInitProjection(core);
  fresh.publishInitProjection(core, retailInitProjection);
  ok &= expect(fresh.retailCentre().x == 0 && fresh.retailCentre().y == 0,
               "the init publication's centre was not read back from the guest's own registers");
  ok &= expect(fresh.publishedScreenDistance() == 1000, "the init publication's H was not recorded");
  ok &= expect(fresh.plan().presentationExtent.width == 428,
               "the init site did not latch the plan for the live display extent");
  ok &= expect(fresh.plan().projectionHorizontalMargin == 54, "the init site latched the wrong margin");

  // 10. No 4:3 width is baked in: a title that published 368 widens 368 -> 492.
  core.game->gpu.s_disp_w = 368;
  publish(0, 0);
  ok &= expect(owner.plan().nativeExtent.width == 368 && owner.plan().presentationExtent.width == 492 &&
                   owner.plan().projectionHorizontalMargin == 62,
               "the owner did not measure the title's own display extent");

  // 11. Guest RAM bound: a fixture of low addresses cannot catch a bound that ignores KSEG0.
  ok &= expect(Crash1Widescreen::isGuestRam(0x800578D0) && Crash1Widescreen::isGuestRam(0x80100000) &&
                   !Crash1Widescreen::isGuestRam(0) && !Crash1Widescreen::isGuestRam(0x80200000),
               "the guest RAM bound accepted an address outside the 2 MiB main RAM");

  ok &= installWithNoHlePlan();

  if (!ok) {
    return 1;
  }
  std::printf("Crash 1 guest widescreen: 4:3 identity exact, 16:9 moves the guest centre by the "
              "margin with OFY and H untouched, the margin cannot compound, and the init "
              "publication is read from the guest\n");
  return 0;
}