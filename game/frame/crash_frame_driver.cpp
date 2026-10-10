#include "crash_frame_driver.h"

#include "core.h"
#include "emulated_time.h"
#include "execution_control.h"
#include "field_rate.h"
#include "game.h"
#include "native_dispatch.h"
#include "title_facts.h"

#include <cstdlib>
#include <limits>
#include <lucent/log.h>

namespace crash {
namespace {

// One host turn: runs the guest from `address` until a typed boundary or the budget ends.
psx::cpu::ExecutionResult executeOneHostTurn(Core &core, std::uint32_t address) {
  return psx::cpu::dispatchGuestUntilExit(core, address, psx::cpu::ExecutionBudget::currentTurn(core));
}

constexpr std::uint32_t kRootCounterSpec = 0xF2000002u;
constexpr std::uint32_t kRootCounterTarget = 0x1000u;
constexpr std::uint64_t kRootCounterCpuTicks = 8ull * kRootCounterTarget;

} // namespace

CrashFrameDriver::CrashFrameDriver(Game &game,
                                   FrameCut &frameCut,
                                   const FrameProgram &program,
                                   const NativeFrameLoopContract &contract)
    : game_(game), frameCut_(frameCut), program_(program), contract_(contract) {}

CrashFrameDriver &CrashFrameDriver::from(Core &core) {
  if (core.game == nullptr || core.game->frameDriver == nullptr) {
    lucent::error("crash-frame", "frame override has no bound FrameDriver");
    std::abort();
  }
  return static_cast<CrashFrameDriver &>(*core.game->frameDriver);
}

void CrashFrameDriver::installOverrides(Game &game, const FrameProgram &program) {
  const struct Binding {
    std::uint32_t address;
    psx::cpu::NativeFunction function;
    const char *owner;
  } bindings[]{
      {program.transition.begin, finishFrameIteration, "CoreLoop transition"},
      {program.setRootCounter, setRootCounterSuper, "SetRCnt"},
      {program.startRootCounter, startRootCounterSuper, "StartRCnt"},
      {program.stopRootCounter, stopRootCounterSuper, "StopRCnt"},
  };
  for (const Binding &binding : bindings) {
    psx::cpu::installNativeOverride(game.core, binding.address, binding.owner, binding.function);
  }
}

void CrashFrameDriver::publishInput(Core &) {}

std::string CrashFrameDriver::armWarp(Core &core, const char *line) {
  return warp_.arm(line, enteredCoreLoop_, core, program_);
}

void CrashFrameDriver::callOriginal(Core &core, std::uint32_t address) {
  psx::cpu::callOriginalToReturn(core, address, psx::cpu::ExecutionBudget::currentTurn(core), "original call");
}

void CrashFrameDriver::resetRootCounterClock(const Core &core) {
  nextRootCounterTick_ = core.game->timing.emulatedCpuTicks() + kRootCounterCpuTicks;
}

void CrashFrameDriver::serviceRootCounter(Core &core, std::uint64_t throughCpuTick) {
  if (!rootCounterConfigured_ || !rootCounterRunning_) {
    return;
  }
  while (nextRootCounterTick_ <= throughCpuTick) {
    const R3000 interrupted = static_cast<const R3000 &>(core);
    psx::cpu::dispatchGuestToReturn(
        core, program_.rootCounterIncrement, psx::cpu::ExecutionBudget::currentTurn(core), "root-counter callback");
    static_cast<R3000 &>(core) = interrupted;
    nextRootCounterTick_ += kRootCounterCpuTicks;
  }
}

void CrashFrameDriver::setRootCounterSuper(Core *core) {
  CrashFrameDriver &driver = from(*core);
  const std::uint32_t spec = core->r[4];
  const std::uint32_t target = core->r[5];
  driver.callOriginal(*core, driver.program_.setRootCounter);
  if (spec == kRootCounterSpec && target == kRootCounterTarget) {
    driver.rootCounterConfigured_ = true;
    driver.resetRootCounterClock(*core);
  }
}

void CrashFrameDriver::startRootCounterSuper(Core *core) {
  CrashFrameDriver &driver = from(*core);
  const std::uint32_t spec = core->r[4];
  driver.callOriginal(*core, driver.program_.startRootCounter);
  if (spec == kRootCounterSpec) {
    if (!driver.rootCounterConfigured_) {
      lucent::error("crash-frame", "StartRCnt(CNT2) preceded the measured SetRCnt(CNT2,0x1000) setup");
      std::abort();
    }
    driver.rootCounterRunning_ = true;
    driver.resetRootCounterClock(*core);
  }
}

void CrashFrameDriver::stopRootCounterSuper(Core *core) {
  CrashFrameDriver &driver = from(*core);
  const std::uint32_t spec = core->r[4];
  if (spec == kRootCounterSpec) {
    driver.serviceRootCounter(*core, core->game->timing.emulatedCpuTicks());
    driver.rootCounterRunning_ = false;
  }
  driver.callOriginal(*core, driver.program_.stopRootCounter);
}

void CrashFrameDriver::deliverDisplayField(Core &core) {
  if (!rootCounterConfigured_ || !rootCounterRunning_) {
    lucent::error("crash-frame", "GpuUpdate reached its display wait before CNT2 was configured and started");
    std::abort();
  }
  if (deliveredFields_ >= 2) {
    lucent::error("crash-frame", "one retail GpuUpdate requested more than its measured two display fields");
    std::abort();
  }
  if (deliveredFields_ == 0) {
    waitBaseCpuTick_ = game_.timing.emulatedCpuTicks();
  }
  ++deliveredFields_;
  const std::uint64_t displayBoundary =
      waitBaseCpuTick_ + psx::frame::displayFieldCpuTicks(deliveredFields_, 1u, psx::frame::FIELD_RATE_NTSC_MILLIHZ);
  serviceRootCounter(core, displayBoundary);
  game_.spu_audio.frame();
}

void CrashFrameDriver::finishFrameIteration(Core *core) {
  CrashFrameDriver &driver = from(*core);
  if (core->r[31] != driver.program_.transitionReturn) {
    lucent::error("crash-frame",
                  "loop transition reached with ra=0x{:08X}; expected the retail return 0x{:08X}",
                  core->r[31],
                  driver.program_.transitionReturn);
    std::abort();
  }
  driver.frameCompleted_ = true;
  // requestExecutionExit stamps no PC; the address is provenance only, the driver resumes from $r31.
  psx::cpu::requestExecutionExit(
      *core,
      psx::cpu::ExecutionResult{
          psx::cpu::ExecutionExitReason::FrameBoundary, driver.contract_.guestVSync.begin, 0, {}});
}

psx::cpu::ExecutionResult
CrashFrameDriver::runGuestToBoundary(Core &core, std::uint32_t entry, const ExecuteSlice &execute) {
  std::uint32_t resumePc = entry;
  std::uint64_t consumedCycles = 0;
  while (true) {
    psx::cpu::ExecutionResult result = execute(core, resumePc);
    if (result.cycles > std::numeric_limits<std::uint64_t>::max() - consumedCycles) {
      return {psx::cpu::ExecutionExitReason::Fault, result.guestPc, consumedCycles, "guest cycle count overflow"};
    }
    consumedCycles += result.cycles;
    if (result.reason != psx::cpu::ExecutionExitReason::BudgetExhausted) {
      result.cycles = consumedCycles;
      return result;
    }
    if (result.cycles == 0u) {
      return {psx::cpu::ExecutionExitReason::Fault,
              result.guestPc,
              consumedCycles,
              "guest turn exhausted its budget without advancing guest cycles"};
    }
    serviceRootCounter(core, game_.timing.emulatedCpuTicks());
    resumePc = result.guestPc;
  }
}

void CrashFrameDriver::stepFrame(Core &core, std::uint32_t frame) {
  if (&core != &game_.core || core.game != &game_) {
    lucent::error("crash-frame", "CrashFrameDriver was asked to step a different Game/Core");
    std::abort();
  }
  if (completedFrames_ != 0 && frame != lastHostFrame_ + 1) {
    lucent::error("crash-frame", "non-sequential frame {} requested after host frame {}", frame, lastHostFrame_);
    std::abort();
  }
  lastHostFrame_ = frame;

  serviceRootCounter(core, game_.timing.emulatedCpuTicks());
  game_.timing.logicFrame = completedFrames_;
  game_.timing.frameTick();
  core.rsub.otAttr.beginLogicFrame(completedFrames_);
  game_.pad.serviceFrame();
  publishInput(core);
  warp_.apply(core, program_);
  deliveredFields_ = 0;
  frameCompleted_ = false;

  psx::cpu::ExecutionResult result;
  if (!enteredCoreLoop_) {
    core.r[4] = program_.initialScene;
    result = runGuestToBoundary(core, program_.coreLoop.begin, executeOneHostTurn);
    enteredCoreLoop_ = true;
  } else {
    result = runGuestToBoundary(core, program_.iteration.begin, executeOneHostTurn);
  }
  while (!frameCompleted_ && result.reason == psx::cpu::ExecutionExitReason::FrameBoundary) {
    if (!CrashFrameDriver::isMeasuredFrameBoundary(result.guestPc,
                                                   core.r[31],
                                                   contract_.guestVSync.begin,
                                                   program_.afterFirstVSync,
                                                   program_.afterVSync,
                                                   program_.transitionReturn)) {
      lucent::error("crash-frame",
                    "frame {} reached an unexpected boundary at 0x{:08X} with ra=0x{:08X}; the measured "
                    "display waits are the guest's own VSync calls returning to 0x{:08X}/0x{:08X}, or "
                    "the transition override returning to 0x{:08X} stamping 0x{:08X}",
                    frame,
                    result.guestPc,
                    core.r[31],
                    program_.afterFirstVSync,
                    program_.afterVSync,
                    program_.transitionReturn,
                    contract_.guestVSync.begin);
      std::abort();
    }
    const std::uint32_t continuation = core.r[31];
    deliverDisplayField(core);
    result = runGuestToBoundary(core, continuation, executeOneHostTurn);
  }
  if (!frameCompleted_ || result.reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    lucent::error("crash-frame",
                  "frame {} left guest execution at 0x{:08X} with {} after {} cycles ({}); "
                  "expected the measured frame boundary",
                  frame,
                  result.guestPc,
                  psx::cpu::executionExitName(result.reason),
                  result.cycles,
                  result.detail);
    std::abort();
  }

  if (core.mem_r32(program_.doneAddress) != 0) {
    lucent::error("crash-frame",
                  "the title requested CoreLoop exit after frame {}; its measured two-GpuUpdate drain/NSKill "
                  "transition is not yet extracted into host turns",
                  frame);
    std::abort();
  }
  if (deliveredFields_ == 0 || deliveredFields_ > 2) {
    lucent::error("crash-frame", "frame {} returned without the measured one-or-two GpuUpdate display waits", frame);
    std::abort();
  }

  frameCut_.observe(core);
  game_.presentation.commit(&core, static_cast<int>(deliveredFields_), game_.temporalPresentation.get());
  ++completedFrames_;
}

} // namespace crash
