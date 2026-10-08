// Falsifiers for the horizontal-bound owner: the two pure rules (near-plane band, light-intensity offset)
// and the owner through the real runtime with a stand-in retail body. Inputs are in
// titles/crash1/executable.json.

#include "crash1_horizontal_bound.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "game_runtime.h"
#include "hw_bind.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "psx_exe_image.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using crash1::Crash1HorizontalBound;

int failures = 0;

void check(bool condition, const char *what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

constexpr std::uint32_t kGoolObjectTransform = 0x8001DE78u;
constexpr std::uint32_t kNearPlaneConsumer = 0x8003A144u;

} // namespace

// Stand-in for FUN_8001DE78 (reads the bound at 0x8001DF6C/0x8001DFFC/0x8001E048); records what it saw.
namespace {

std::vector<std::int32_t> bodyObserved;

void retailSubmitter(Core &core) {
  bodyObserved.push_back(static_cast<std::int32_t>(core.mem_r32(crash1::kHorizontalBound)));
}

// A body that moves the bound, for the negative case.
void boundMovingSubmitter(Core &core) {
  bodyObserved.push_back(static_cast<std::int32_t>(core.mem_r32(crash1::kHorizontalBound)));
  // Differs from the handed value, so the move is real.
  core.mem_w32(crash1::kHorizontalBound, 0x3E8u);
}

} // namespace

int main() {
  // The pure rules.
  // FUN_8003A144 [0x8003A144,0x8003A76C): reject unless H < Z (0x8003A240), reject if 11999 < Z (0x8003A248);
  // Z is the rtps depth at 0x8003A220.
  check(Crash1HorizontalBound::insideNearPlane(1000, 1001), "H=1000 admits the first depth past the plane");
  check(!Crash1HorizontalBound::insideNearPlane(1000, 1000), "H=1000 rejects the projection plane itself");
  check(!Crash1HorizontalBound::insideNearPlane(1000, 999), "H=1000 rejects a depth in front of the plane");
  check(Crash1HorizontalBound::insideNearPlane(1000, 11999), "the last accepted depth is 11999");
  check(!Crash1HorizontalBound::insideNearPlane(1000, 12000),
        "12000 is past the far limit the arm at 0x8003A248 materialises");
  // A nearer plane admits a shallower depth, so H differs per camera.
  check(Crash1HorizontalBound::insideNearPlane(500, 600), "a nearer H admits a depth the farther H rejected");
  check(!Crash1HorizontalBound::insideNearPlane(1000, 600), "the same depth is behind the farther plane");

  // A widening holds H, so the band must answer identically before and after.
  for (const std::int32_t bound : {288, 460, 500, 800, 960, 1000}) {
    for (std::int32_t depth = 0; depth <= 12002; ++depth) {
      check(Crash1HorizontalBound::insideNearPlane(bound, depth) == (bound < depth && depth < 12000),
            "the near-plane band is the title's own comparison, for every measured bound");
    }
  }

  // `(object+0x138) + 0x800 - H/2`; the halving rounds toward zero (`srl/sra` at 0x8001E008/0x8001E010),
  // unlike C division on a negative bound.
  check(Crash1HorizontalBound::lightIntensityOffset(0, 1000) == 0x800 - 500,
        "the offset is object + 0x800 - H/2 at the boot bound");
  check(Crash1HorizontalBound::lightIntensityOffset(0, 1001) == 0x800 - 500,
        "an odd bound halves toward zero, as the title's sra does");
  check(Crash1HorizontalBound::lightIntensityOffset(0, -1001) == 0x800 - (-500),
        "a negative bound also halves toward zero, so the sign-extension pair matters");
  check(Crash1HorizontalBound::lightIntensityOffset(0, -1000) == 0x800 + 500, "an even negative bound is exact");

  // The five per-camera values the single writer publishes, plus gte_init's 1000.
  for (const std::int32_t retail : {1000, 500, 960, 800, 460, 288}) {
    check(crash1::isRetailHorizontalBound(retail), "a measured camera bound is accepted");
  }
  for (const std::int32_t foreign : {0, 320, 1024, -1000, 1378}) {
    check(!crash1::isRetailHorizontalBound(foreign), "a value the camera setup cannot publish is refused");
  }

  // The owner, through the real runtime.
  crash1::Crash1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  if (game->core.runtime != &runtime) {
    std::fprintf(stderr, "FAIL: the runtime did not install onto the Core\n");
    return 1;
  }
  // Activate the image first: an override key is (image identity, guest address).
  const auto identity = game->core.imageCatalog().activate("Crash1HorizontalBound", {0x00010000u, 0x00056800u}, 1u);
  crash1::installCrash1HorizontalBound(game->core);

  check(game->core.nativeDispatcher().isInstalled({identity, kGoolObjectTransform}),
        "the owner installed its override on the measured submitter entry");
  check(!game->core.nativeDispatcher().isInstalled({identity, kNearPlaneConsumer}),
        "the near-plane consumer is read as evidence, not overwritten: overriding it would replace "
        "the title's own visibility decision rather than check the bound it was given");

  Crash1HorizontalBound &owner = runtime.horizontalBound();
  bodyObserved.clear();
  game->core.mem_w32(crash1::kHorizontalBound, 1000u);
  owner.observeSubmitter(game->core, retailSubmitter);
  check(bodyObserved == std::vector<std::int32_t>{1000}, "the retail body ran and saw the bound itself");
  check(owner.submissions() == 1, "one submission is counted");
  check(owner.observedBounds() == std::vector<std::int32_t>{1000}, "the consumed bound is recorded once");
  check(owner.lastObservedBound() == 1000, "the last consumed bound is the one the body read");

  // Repeated submissions at one bound keep the distinct set at one; the denominator is asserted with it.
  owner.observeSubmitter(game->core, retailSubmitter);
  owner.observeSubmitter(game->core, retailSubmitter);
  check(owner.submissions() == 3, "every submission is counted");
  check(owner.observedBounds() == std::vector<std::int32_t>{1000}, "a repeated bound is not appended twice");

  // A per-camera change is recorded in first-seen order.
  game->core.mem_w32(crash1::kHorizontalBound, 500u);
  owner.observeSubmitter(game->core, retailSubmitter);
  check(owner.submissions() == 4, "the camera change is a submission like any other");
  check(owner.observedBounds() == std::vector<std::int32_t>{1000, 500}, "a new bound is appended once");
  check(bodyObserved.size() == 4, "the body saw all four submissions, so the owner did not skip it");

  // The owner aborts on both refusals, so the decisions are pinned directly.
  // 1. A body that moves the bound.
  check(Crash1HorizontalBound::boundIntact(800, 800), "an unmoved bound is intact");
  check(!Crash1HorizontalBound::boundIntact(800, 1000), "a body that moves the bound breaks the contract");
  check(!Crash1HorizontalBound::boundIntact(1000, 0), "clearing the bound is a break, not an intact zero");
  // 2. A bound the camera setup cannot publish (e.g. a widening done by scaling H).
  check(!crash1::isRetailHorizontalBound(1001), "a bound one above a measured one is refused");
  check(!crash1::isRetailHorizontalBound(0), "a cleared bound is refused");
  check(crash1::isRetailHorizontalBound(1000), "the boot bound is accepted");

  // The moving body is not run against the owner; this only shows the break the owner refuses.
  bodyObserved.clear();
  game->core.mem_w32(crash1::kHorizontalBound, 800u);
  boundMovingSubmitter(game->core);
  check(bodyObserved == std::vector<std::int32_t>{800}, "the moving body did read the bound it was given");
  check(
      !Crash1HorizontalBound::boundIntact(800, static_cast<std::int32_t>(game->core.mem_r32(crash1::kHorizontalBound))),
      "and it left the bound moved, which is the break the owner refuses");

  std::printf("Crash1HorizontalBound: %llu submission(s), %zu distinct bound(s) {%s}; near-plane band and "
              "light-intensity offset pinned; bound is read pre-GTE and never written by the owner\n",
              static_cast<unsigned long long>(owner.submissions()),
              owner.observedBounds().size(),
              owner.observedBounds().empty() ? "" : "1000,500");
  if (failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}
