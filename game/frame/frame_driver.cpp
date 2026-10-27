#include "frame/frame_driver.h"

#include "core.h"
#include "emulated_time.h"
#include "execution_control.h"
#include "field_rate.h"
#include "game.h"
#include "logic_frame.h"
#include "guest_call.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <limits>
#include <lucent/log.h>
#include <utility>

namespace crash {
namespace {

constexpr std::uint32_t kRootCounterSpec = 0xF2000002u;
// CNT2 counts the system clock divided by eight.
constexpr std::uint64_t kRootCounterClockDivider = 8u;

std::uint64_t fieldCpuTicks(std::uint32_t fields) {
  return psx::frame::displayFieldCpuTicks(fields, 1u, psx::frame::FIELD_RATE_NTSC_MILLIHZ);
}

} // namespace

psx::cpu::ExecutionResult FrameDriver::executeHostTurn(Core &core, std::uint32_t address) {
  return psx::cpu::dispatchGuestUntilExitNow(core, address);
}

FrameDriver::FrameDriver(
    Game &game, FrameCut &frameCut, const FrameProgram &program, std::uint32_t vsyncEntry, ExecuteSlice execute)
    : game_(game), frameCut_(frameCut), program_(program), vsyncEntry_(vsyncEntry), execute_(std::move(execute)),
      displayBoundaryTick_(game.timing.emulatedCpuTicks()) {
  frameCut_.reset();
}

void FrameDriver::installOverrides(Game &game, const FrameProgram &program) {
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

void FrameDriver::publishInput(Core &) {}

std::string FrameDriver::armWarp(Core &core, std::uint32_t scene) {
  return warp_.arm(scene, core, program_);
}

void FrameDriver::callOriginal(Core &core, std::uint32_t address) {
  psx::cpu::callOriginalToReturn(core, address, psx::cpu::ExecutionBudget::currentTurn(core), "original call");
}

void FrameDriver::resetRootCounterClock(const Core &core) {
  nextRootCounterTick_ = core.game->timing.emulatedCpuTicks() + rootCounterPeriod_;
}

void FrameDriver::serviceRootCounter(Core &core, std::uint64_t throughCpuTick) {
  if (!rootCounterConfigured_ || !rootCounterRunning_) {
    return;
  }
  while (nextRootCounterTick_ <= throughCpuTick) {
    const R3000 interrupted = static_cast<const R3000 &>(core);
    psx::cpu::dispatchGuestToReturn(
        core, program_.rootCounterIncrement, psx::cpu::ExecutionBudget::currentTurn(core), "root-counter callback");
    static_cast<R3000 &>(core) = interrupted;
    nextRootCounterTick_ += rootCounterPeriod_;
  }
}

void FrameDriver::setRootCounterSuper(Core *core) {
  FrameDriver &driver = core->game->frameDriverAs<FrameDriver>();
  const std::uint32_t spec = core->r[4];
  const std::uint32_t target = core->r[5];
  driver.callOriginal(*core, driver.program_.setRootCounter);
  if (spec == kRootCounterSpec && target != 0u) {
    driver.rootCounterConfigured_ = true;
    driver.rootCounterPeriod_ = kRootCounterClockDivider * target;
    driver.resetRootCounterClock(*core);
  }
}

void FrameDriver::startRootCounterSuper(Core *core) {
  FrameDriver &driver = core->game->frameDriverAs<FrameDriver>();
  const std::uint32_t spec = core->r[4];
  driver.callOriginal(*core, driver.program_.startRootCounter);
  if (spec == kRootCounterSpec) {
    if (!driver.rootCounterConfigured_) {
      lucent::error("crash-frame", "StartRCnt(CNT2) preceded the measured SetRCnt(CNT2) setup");
      std::abort();
    }
    driver.rootCounterRunning_ = true;
    driver.resetRootCounterClock(*core);
  }
}

void FrameDriver::stopRootCounterSuper(Core *core) {
  FrameDriver &driver = core->game->frameDriverAs<FrameDriver>();
  const std::uint32_t spec = core->r[4];
  if (spec == kRootCounterSpec) {
    driver.serviceRootCounter(*core, core->game->timing.emulatedCpuTicks());
    driver.rootCounterRunning_ = false;
  }
  driver.callOriginal(*core, driver.program_.stopRootCounter);
}

void FrameDriver::deliverField(Core &core, std::uint64_t boundaryCpuTick) {
  if (!rootCounterConfigured_ || !rootCounterRunning_) {
    lucent::error("crash-frame", "a display field was delivered before CNT2 was configured and started");
    std::abort();
  }
  ++deliveredFields_;
  serviceRootCounter(core, boundaryCpuTick);
  game_.spu_audio.frame();
}

void FrameDriver::deliverDisplayWait(Core &core, Boundary wait) {
  const bool gpuUpdate = wait == Boundary::GpuUpdateWait;
  WaitGroup &group = gpuUpdate ? gpuUpdateWaits_ : loadingSwapWaits_;
  // GpuUpdate waits at most twice; the loader's loading picture swaps both buffers.
  if (group.fields >= 2) {
    lucent::error("crash-frame",
                  "one retail {} requested more than its measured two display fields",
                  gpuUpdate ? "GpuUpdate" : "loading swap");
    std::abort();
  }
  if (group.fields == 0) {
    group.baseCpuTick = game_.timing.emulatedCpuTicks();
  }
  ++group.fields;
  deliverField(core, group.baseCpuTick + fieldCpuTicks(group.fields));
}

bool FrameDriver::limiterSkippedWaits(Core &core) const {
  if (!program_.limiter) {
    return false;
  }
  const FrameLimiter &limiter = *program_.limiter;
  const std::uint32_t stamp = core.mem_r32(core.mem_r32(limiter.displayedFrame) + limiter.stampOffset);
  return core.mem_r32(limiter.vblankCounter) - stamp >= limiter.skipAfterFields;
}

void FrameDriver::deliverSkippedWaits(Core &core, std::uint32_t frame) {
  const std::uint64_t elapsed = game_.timing.emulatedCpuTicks() - displayBoundaryTick_;
  if (!limiterSkippedWaits(core) || elapsed == 0u) {
    lucent::error(
        "crash-frame", "frame {} reached the loop transition with no display wait and no measured limiter skip", frame);
    std::abort();
  }
  // The picture shows at the first VBlank at or after the swap; the fields up to it belong to this frame.
  std::uint32_t fields = static_cast<std::uint32_t>(elapsed / fieldCpuTicks(1u));
  while (fieldCpuTicks(fields) < elapsed) {
    ++fields;
  }
  for (std::uint32_t field = 1; field <= fields; ++field) {
    deliverField(core, displayBoundaryTick_ + fieldCpuTicks(field));
  }
}

void FrameDriver::finishFrameIteration(Core *core) {
  FrameDriver &driver = core->game->frameDriverAs<FrameDriver>();
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
      *core, psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::FrameBoundary, driver.vsyncEntry_, 0, {}});
}

psx::cpu::ExecutionResult
FrameDriver::runGuestToBoundary(Core &core, std::uint32_t entry, const ExecuteSlice &execute) {
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

void FrameDriver::stepFrame(Core &core, std::uint32_t frame) {
  if (&core != &game_.core || core.game != &game_) {
    lucent::error("crash-frame", "the frame driver was asked to step a different Game/Core");
    std::abort();
  }
  if (completedFrames_ != 0 && frame != lastHostFrame_ + 1) {
    lucent::error("crash-frame", "non-sequential frame {} requested after host frame {}", frame, lastHostFrame_);
    std::abort();
  }
  lastHostFrame_ = frame;

  serviceRootCounter(core, game_.timing.emulatedCpuTicks());
  game_.timing.frameTick();
  psx::frame::LogicFrame::begin(game_, completedFrames_);
  publishInput(core);
  warp_.apply(core, program_);
  deliveredFields_ = 0;
  gpuUpdateWaits_ = {};
  loadingSwapWaits_ = {};
  frameCompleted_ = false;

  psx::cpu::ExecutionResult result;
  if (!enteredCoreLoop_) {
    core.r[4] = program_.initialScene;
    result = runGuestToBoundary(core, program_.coreLoop.begin, execute_);
    enteredCoreLoop_ = true;
  } else {
    result = runGuestToBoundary(core, program_.iteration.begin, execute_);
  }
  while (!frameCompleted_ && result.reason == psx::cpu::ExecutionExitReason::FrameBoundary) {
    const Boundary boundary = classifyBoundary(result.guestPc, core.r[31], vsyncEntry_, program_);
    if (boundary != Boundary::GpuUpdateWait && boundary != Boundary::LoadingSwapWait) {
      lucent::error("crash-frame",
                    "frame {} reached an unexpected boundary at 0x{:08X} with ra=0x{:08X}; the measured "
                    "display waits are the guest's own VSync calls returning to 0x{:08X}/0x{:08X} or to the "
                    "loading swap's 0x{:08X}, or the transition override returning to 0x{:08X} stamping 0x{:08X}",
                    frame,
                    result.guestPc,
                    core.r[31],
                    program_.afterFirstVSync,
                    program_.afterVSync,
                    program_.loadingSwap ? program_.loadingSwap->afterVSync : 0u,
                    program_.transitionReturn,
                    vsyncEntry_);
      std::abort();
    }
    const std::uint32_t continuation = core.r[31];
    deliverDisplayWait(core, boundary);
    result = runGuestToBoundary(core, continuation, execute_);
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
  if (deliveredFields_ == 0) {
    deliverSkippedWaits(core, frame);
  }
  frameCut_.observe(core);
  game_.presentation.commit(&core, static_cast<int>(deliveredFields_));
  displayBoundaryTick_ = game_.timing.emulatedCpuTicks();
  ++completedFrames_;
}

} // namespace crash
