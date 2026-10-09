#include "core.h"
#include "crash_frame_driver.h"
#include "crash_titles.h"
#include "game.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

constexpr std::uint32_t kEntry = 0x80010000u;
constexpr std::uint32_t kLoop = 0x80010004u;
constexpr std::uint32_t kBoundary = 0x80010008u;

bool expect(const char *title, bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL %s: %s\n", title, message);
  }
  return condition;
}

// The display-wait predicate accepts the guest's own VSync calls and the loop transition override, nothing else.
bool boundaryPredicateHolds(const char *title, const crash::TitleFacts &facts) {
  const crash::FrameProgram &f = facts.frame;
  const std::uint32_t leaf = facts.contract.guestVSync.begin;
  const auto measured = [&](std::uint32_t pc, std::uint32_t ra) {
    return crash::CrashFrameDriver::isMeasuredFrameBoundary(
        pc, ra, leaf, f.afterFirstVSync, f.afterVSync, f.transitionReturn);
  };
  bool ok = true;
  ok = expect(title, measured(f.afterFirstVSync, f.afterFirstVSync), "the first VSync call is a boundary") && ok;
  ok = expect(title, measured(f.afterVSync, f.afterVSync), "the second VSync call is a boundary") && ok;
  ok = expect(title, measured(leaf, f.transitionReturn), "the transition override's provenance is a boundary") && ok;
  ok = expect(title, !measured(f.afterFirstVSync, f.transitionReturn), "provenances do not mix") && ok;
  ok = expect(title, !measured(leaf, leaf), "the VSync leaf entry is never a resume point") && ok;
  ok =
      expect(title, !measured(f.gpuUpdate.begin, f.gpuUpdate.begin), "an arbitrary GpuUpdate address is refused") && ok;
  return ok;
}

} // namespace

int main() {
  const int failures = crash_test::forEachTitle([](const char *title, const crash::TitleFacts &facts) {
    if (!boundaryPredicateHolds(title, facts)) {
      return 1;
    }
    auto game = std::make_unique<Game>();
    auto &driver = static_cast<crash::CrashFrameDriver &>(*game->frameDriver);
    Core &core = game->core;
    std::array<std::uint32_t, 3> seen{};
    std::size_t calls = 0;
    const auto completed = driver.runGuestToBoundary(core, kEntry, [&](Core &current, std::uint32_t pc) {
      seen.at(calls++) = pc;
      if (calls == 1) {
        current.r[2] = 11u;
        return psx::cpu::ExecutionResult{
            psx::cpu::ExecutionExitReason::BudgetExhausted, kLoop, 564480u, "cycle budget exhausted"};
      }
      if (calls == 2) {
        current.r[2] = 12u;
        return psx::cpu::ExecutionResult{
            psx::cpu::ExecutionExitReason::BudgetExhausted, kLoop, 100u, "cycle budget exhausted"};
      }
      return psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::FrameBoundary, kBoundary, 80u, "boundary"};
    });
    bool ok = expect(title,
                     calls == 3 && seen == std::array{kEntry, kLoop, kLoop},
                     "turn did not resume both budget exits at their exact guest PCs");
    ok &= expect(title,
                 completed.reason == psx::cpu::ExecutionExitReason::FrameBoundary && completed.guestPc == kBoundary &&
                     completed.cycles == 564660u && core.r[2] == 12u,
                 "turn did not preserve guest state and cumulative cycles through its frame boundary");

    calls = 0;
    const auto stalled = driver.runGuestToBoundary(core, kEntry, [&](Core &, std::uint32_t) {
      ++calls;
      return psx::cpu::ExecutionResult{
          psx::cpu::ExecutionExitReason::BudgetExhausted, kEntry, 0u, "host dispatch budget exhausted"};
    });
    ok &= expect(title,
                 calls == 1 && stalled.reason == psx::cpu::ExecutionExitReason::Fault && stalled.guestPc == kEntry &&
                     stalled.cycles == 0u,
                 "turn repeated a zero-progress budget exit");

    calls = 0;
    const auto fault = driver.runGuestToBoundary(core, kEntry, [&](Core &, std::uint32_t pc) {
      ++calls;
      return psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::Fault, pc, 17u, "synthetic fault"};
    });
    ok &= expect(title,
                 calls == 1 && fault.reason == psx::cpu::ExecutionExitReason::Fault && fault.guestPc == kEntry &&
                     fault.cycles == 17u && fault.detail == "synthetic fault",
                 "turn retried or swallowed a backend fault");
    return ok ? 0 : 1;
  });
  std::printf("frame turn: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
