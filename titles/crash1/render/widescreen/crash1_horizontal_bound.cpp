#include "crash1_horizontal_bound.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash1 {
namespace {

[[noreturn]] void refuse(const char *what, std::int32_t observed) {
  lucent::error("crash1-bound",
                "SCUS-949.00 horizontal bound {}; the guest's own camera setup cannot publish it, so "
                "something moved the bound a widening must hold fixed",
                what,
                observed);
  std::abort();
}

void originalSubmitter(Core &core) {
  psx::cpu::callOriginalToReturn(
      core, kHorizontalSubmitter, psx::cpu::ExecutionBudget::currentTurn(core), "crash1-bound::submitter original");
}

void submitterOverride(Core *core) {
  Crash1HorizontalBound::from(*core).observeSubmitter(*core, originalSubmitter);
}

} // namespace

void Crash1HorizontalBound::observeSubmitter(Core &core, const RetailBody &retail) {
  if (!retail) {
    refuse("observation reached without the retail submitter body", 0);
  }
  // Pre-GTE state: the bound FUN_8001DE78 consumes at 0x8001DF6C/0x8001DFFC/0x8001E048.
  const auto consumed = static_cast<std::int32_t>(core.mem_r32(kHorizontalBound));
  if (!isRetailHorizontalBound(consumed)) {
    refuse("is not a value the measured camera setup publishes", consumed);
  }

  retail(core);

  // The retail submitter does not write the bound; a change is reported, not re-read.
  const auto after = static_cast<std::int32_t>(core.mem_r32(kHorizontalBound));
  if (!Crash1HorizontalBound::boundIntact(consumed, after)) {
    refuse("was moved by the submitter it is supposed to read", after);
  }

  // Distinct bounds in first-seen order; read with submissions() as the denominator.
  bool already = false;
  for (const std::int32_t seen : observedBounds_) {
    already = already || seen == consumed;
  }
  if (!already) {
    observedBounds_.push_back(consumed);
  }
  lastObservedBound_ = consumed;
  ++submissions_;
}

Crash1HorizontalBound &Crash1HorizontalBound::from(Core &core) {
  if (!core.runtime) {
    lucent::error("crash1-bound", "SCUS-949.00 horizontal-bound override ran without its title runtime");
    std::abort();
  }
  // The const is dropped because this override increments per-title counters.
  auto *const runtime = dynamic_cast<Crash1Runtime *>(const_cast<GameRuntime *>(core.runtime));
  if (!runtime) {
    lucent::error("crash1-bound", "SCUS-949.00 horizontal-bound override reached another title's runtime");
    std::abort();
  }
  return runtime->horizontalBound();
}

void installCrash1HorizontalBound(Core &core) {
  psx::cpu::installNativeOverride(core, kHorizontalSubmitter, "Crash GoolObjectTransform bound", submitterOverride);
  lucent::info("crash1-bound",
               "horizontal-bound owner installed: submitter 0x{:08X} over the bound at 0x{:08X} "
               "(written only at 0x{:08X}, re-sent to CR[26] at 0x{:08X}); near-plane consumer "
               "0x{:08X} far limit {}",
               kHorizontalSubmitter,
               kHorizontalBound,
               kHorizontalBoundWriter,
               kHorizontalBoundResend,
               kHorizontalNearPlane,
               kHorizontalFarLimit);
}

} // namespace crash1
