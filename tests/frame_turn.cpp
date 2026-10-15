#include "core.h"
#include "emulated_time.h"
#include "field_rate.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "runtime/session.h"
#include "titles.h"

#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>

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
  const std::uint32_t leaf = facts.vsync.begin;
  const auto measured = [&](std::uint32_t pc, std::uint32_t ra) {
    return crash::FrameDriver::isMeasuredFrameBoundary(
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

constexpr std::uint32_t kRootCounterSpec = 0xF2000002u;
constexpr std::uint32_t kCrash3RootCounterTarget = 0x1010u;
// Free RAM the test's root-counter callback counts into, and where the limiter's frame record sits.
constexpr std::uint32_t kCallbackCount = 0x800F0000u;
constexpr std::uint32_t kDisplayedFrame = 0x800F1000u;

std::uint64_t fieldTicks(std::uint32_t fields) {
  return psx::frame::displayFieldCpuTicks(fields, 1u, psx::frame::FIELD_RATE_NTSC_MILLIHZ);
}

void writeReturnStub(Core &core, std::uint32_t address) {
  core.mem_w32(address, 0x03E00008u); // jr $ra
  core.mem_w32(address + 4u, 0u);
}

// The callback bumps a RAM word, so the test counts CNT2 targets through the real guest dispatch.
void writeCountingCallback(Core &core, std::uint32_t address) {
  const std::array<std::uint32_t, 6> words{
      0x3C08800Fu, // lui $t0, 0x800F
      0x8D090000u, // lw $t1, 0($t0)
      0x00000000u, // nop
      0x25290001u, // addiu $t1, $t1, 1
      0x03E00008u, // jr $ra
      0xAD090000u, // sw $t1, 0($t0)
  };
  for (std::size_t index = 0; index < words.size(); ++index) {
    core.mem_w32(address + static_cast<std::uint32_t>(4u * index), words.at(index));
  }
}

// One host frame of a scripted guest: `waits` VSync calls in GpuUpdate, `ticks` of guest time before them.
struct FrameScript {
  std::uint32_t waits;
  std::uint64_t ticks;
};

// A FrameDriver whose guest turns follow `script`, bound as the Game's driver with CNT2 set up by the title's
// own SetRCnt/StartRCnt overrides.
class ScriptedFrames {
public:
  ScriptedFrames(Game &game, const crash::TitleFacts &facts) : game_(game), facts_(facts) {
    Core &core = game.core;
    const GuestAddressRange resident{0x00010000u, facts.crt0.bss.begin & 0x1FFFFFFFu};
    identity_ = core.imageCatalog().activate("frame-turn", resident, 1u);
    game.runtime.registerOverrides(game);
    const crash::FrameProgram &program = facts.frame;
    writeReturnStub(core, program.setRootCounter);
    writeReturnStub(core, program.startRootCounter);
    writeCountingCallback(core, program.rootCounterIncrement);
    game.frameDriver = std::make_unique<crash::FrameDriver>(
        game, crash::session(core).frameCut, program, facts.vsync.begin, [this](Core &current, std::uint32_t pc) {
          return step(current, pc);
        });
  }

  crash::FrameDriver &driver() {
    return static_cast<crash::FrameDriver &>(*game_.frameDriver);
  }

  void startRootCounter(std::uint32_t target) {
    Core &core = game_.core;
    core.r[4] = kRootCounterSpec;
    core.r[5] = target;
    core.nativeDispatcher().invoke({identity_, facts_.frame.setRootCounter});
    core.r[4] = kRootCounterSpec;
    core.nativeDispatcher().invoke({identity_, facts_.frame.startRootCounter});
  }

  // Runs host frame `frame` and returns how far its commit moved the display boundary, in fields.
  std::uint64_t run(std::uint32_t frame, FrameScript script) {
    script_ = script;
    calls_ = 0;
    const std::uint64_t before = boundary();
    driver().stepFrame(game_.core, frame);
    return boundary() - before;
  }

private:
  std::uint64_t boundary() const {
    return game_.timing.clockSnapshot().displayBoundaryQ32 >> 32u;
  }

  psx::cpu::ExecutionResult step(Core &core, std::uint32_t) {
    const crash::FrameProgram &program = facts_.frame;
    const std::uint32_t call = calls_++;
    if (call == 0u) {
      game_.timing.advanceGuestInstructionTicks(static_cast<std::uint32_t>(script_.ticks));
    }
    if (call < script_.waits) {
      const std::uint32_t after = call % 2u == 0u ? program.afterFirstVSync : program.afterVSync;
      core.r[31] = after;
      return {psx::cpu::ExecutionExitReason::FrameBoundary, after, 100u, "VSync"};
    }
    core.r[31] = program.transitionReturn;
    const auto transition = core.nativeDispatcher().invoke({identity_, program.transition.begin});
    if (!transition) {
      return {psx::cpu::ExecutionExitReason::Fault, program.transition.begin, 0u, "no transition override"};
    }
    return *transition;
  }

  Game &game_;
  const crash::TitleFacts &facts_;
  psx::cpu::ImageIdentity identity_{};
  FrameScript script_{};
  std::uint32_t calls_{};
};

// Whether `body` stops the process: the driver's refusals abort.
template <typename Body> bool aborts(Body &&body) {
  const pid_t child = fork();
  if (child == 0) {
    std::freopen("/dev/null", "w", stderr);
    body();
    std::_Exit(0);
  }
  int status = 0;
  waitpid(child, &status, 0);
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

// CNT2's period is eight system clocks per SetRCnt target; Crash 3 programs 0x1010.
bool rootCounterFollowsTarget(const char *title, Game &game, const crash::TitleFacts &facts) {
  ScriptedFrames frames(game, facts);
  frames.startRootCounter(kCrash3RootCounterTarget);
  const std::uint64_t period = 8ull * kCrash3RootCounterTarget;
  game.core.mem_w32(kCallbackCount, 0u);
  const auto advanceTo = [&](std::uint64_t ticks) {
    bool first = true;
    frames.driver().runGuestToBoundary(game.core, kEntry, [&](Core &, std::uint32_t pc) {
      if (first) {
        first = false;
        game.timing.advanceGuestInstructionTicks(static_cast<std::uint32_t>(ticks));
        return psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::BudgetExhausted, pc, ticks, "budget"};
      }
      return psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::FrameBoundary, kBoundary, 1u, "boundary"};
    });
  };
  advanceTo(3u * period - 1u);
  bool ok = expect(title, game.core.mem_r32(kCallbackCount) == 2u, "CNT2 fired before its third 8*0x1010 period");
  advanceTo(1u);
  ok &= expect(title, game.core.mem_r32(kCallbackCount) == 3u, "CNT2 missed its third 8*0x1010 period");
  return ok;
}

// Each display wait is one field of the commit; a frame with none is a limiter skip or a refusal.
bool waitsBecomeFields(const char *title, Game &game, const crash::TitleFacts &facts) {
  ScriptedFrames frames(game, facts);
  frames.startRootCounter(kCrash3RootCounterTarget);
  const std::uint64_t field = fieldTicks(1u);
  // Commits land on field boundaries, so a one-tick difference is Q32 rounding.
  const auto fieldsAre = [&](std::uint64_t moved, std::uint32_t fields) {
    const std::uint64_t want = fieldTicks(fields);
    return moved + 1u >= want && moved <= want + 1u;
  };
  bool ok = expect(title, fieldsAre(frames.run(1u, {1u, field / 4u}), 1u), "one display wait did not commit a field");
  ok &= expect(title, fieldsAre(frames.run(2u, {2u, field / 4u}), 2u), "two display waits did not commit two fields");
  ok &= expect(title,
               aborts([&] {
                 frames.run(3u, {3u, field / 4u});
               }),
               "a third display wait was accepted");

  Core &core = game.core;
  if (!facts.frame.limiter) {
    ok &= expect(title,
                 aborts([&] {
                   frames.run(3u, {0u, field / 4u});
                 }),
                 "a frame with no wait was accepted");
    return ok;
  }
  const crash::FrameLimiter &limiter = *facts.frame.limiter;
  core.mem_w32(limiter.displayedFrame, kDisplayedFrame);
  core.mem_w32(kDisplayedFrame + limiter.stampOffset, 40u);
  core.mem_w32(limiter.vblankCounter, 40u + limiter.skipAfterFields - 1u);
  ok &= expect(title,
               aborts([&] {
                 frames.run(3u, {0u, field / 4u});
               }),
               "a frame with no wait was accepted while the limiter still owed one");
  core.mem_w32(limiter.vblankCounter, 40u + limiter.skipAfterFields);
  ok &= expect(title,
               fieldsAre(frames.run(3u, {0u, field + field / 2u}), 2u),
               "a limiter skip after 1.5 fields of guest time did not commit the two fields up to its VBlank");
  ok &= expect(title,
               fieldsAre(frames.run(4u, {0u, field / 4u}), 1u),
               "a short limiter skip did not commit the one field up to its VBlank");
  return ok;
}

} // namespace

int main() {
  int failures = crash_test::forEachTitle([](const char *title, Game &game, const crash::TitleFacts &facts) {
    if (!boundaryPredicateHolds(title, facts)) {
      return 1;
    }
    auto &driver = static_cast<crash::FrameDriver &>(*game.frameDriver);
    Core &core = game.core;
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
  const int clocked = crash_test::forEachTitle([](const char *title, Game &game, const crash::TitleFacts &facts) {
    return rootCounterFollowsTarget(title, game, facts) ? 0 : 1;
  });
  const int fielded = crash_test::forEachTitle([](const char *title, Game &game, const crash::TitleFacts &facts) {
    return waitsBecomeFields(title, game, facts) ? 0 : 1;
  });
  failures += clocked + fielded;
  std::printf("frame turn: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
