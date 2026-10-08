#include "crash1_frame_driver.h"

#include "core.h"
#include "crash1_bios_pad_input.h"
#include "emulated_time.h"
#include "execution_control.h"
#include "field_rate.h"
#include "game.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <limits>
#include <lucent/log.h>

#ifndef CRASH_TITLE_VSYNC_ENTRY
#error "CRASH_TITLE_VSYNC_ENTRY must come from titles/crash1/executable.json"
#endif
#ifndef CRASH_TITLE_VSYNC_END
#error "CRASH_TITLE_VSYNC_END must come from titles/crash1/executable.json"
#endif
#ifndef CRASH1_CORE_LOOP_ENTRY
#error "CRASH1 frame facts must come from titles/crash1/executable.json"
#endif

namespace crash1 {
namespace {

// One host turn: runs the guest from `address` until a typed boundary or the budget ends.
psx::cpu::ExecutionResult executeOneHostTurn(Core &core, std::uint32_t address) {
  return psx::cpu::dispatchGuestUntilExit(core, address, psx::cpu::ExecutionBudget::currentTurn(core));
}

constexpr std::uint32_t kRootCounterSpec = 0xF2000002u;
constexpr std::uint32_t kRootCounterTarget = 0x1000u;
constexpr std::uint64_t kRootCounterCpuTicks = 8ull * kRootCounterTarget;

constexpr crash::NativeFrameLoopContract kContract{
    .codeword = "SCUS-94900",
    .guestVSync = {CRASH_TITLE_VSYNC_ENTRY, CRASH_TITLE_VSYNC_END},
    .vsyncQueryCounter = CRASH_TITLE_VSYNC_QUERY_COUNTER,
    .state = crash::NativeFrameLoopState::FiniteBootSeamOnly,
    .refusal = "candidate frame step is not ready until a real product frame returns",
};

constexpr Crash1FrameProgram kProgram{
    .coreLoop = {CRASH1_CORE_LOOP_ENTRY, CRASH1_CORE_LOOP_END},
    .iteration = {CRASH1_FRAME_ITERATION_ENTRY, CRASH1_FRAME_ITERATION_END},
    .transition = {CRASH1_FRAME_TRANSITION_ENTRY, CRASH1_FRAME_TRANSITION_END},
    .gpuUpdate = {CRASH1_GPU_UPDATE_ENTRY, CRASH1_GPU_UPDATE_END},
    .firstVSync = CRASH1_FIRST_VSYNC,
    .afterFirstVSync = CRASH1_AFTER_FIRST_VSYNC,
    .secondVSync = CRASH1_SECOND_VSYNC,
    .afterVSync = CRASH1_AFTER_VSYNC,
    .doneAddress = CRASH1_DONE_ADDRESS,
    .ticksElapsedAddress = CRASH1_TICKS_ELAPSED_ADDRESS,
    .displayContextAddress = CRASH1_DISPLAY_CONTEXT_ADDRESS,
    .rootCounterIncrement = CRASH1_ROOT_COUNTER_INCREMENT,
    .setRootCounter = CRASH1_SET_ROOT_COUNTER,
    .startRootCounter = CRASH1_START_ROOT_COUNTER,
    .stopRootCounter = CRASH1_STOP_ROOT_COUNTER,
};

static_assert(kContract.guestVSync.valid());
static_assert(kContract.vsyncQueryCounter != 0, "the libetc field counter is measured, not optional");
static_assert(kProgram.coreLoop.valid() && kProgram.iteration.valid() && kProgram.transition.valid() &&
              kProgram.gpuUpdate.valid());
static_assert(kProgram.coreLoop.contains(kProgram.iteration.begin));
static_assert(kProgram.coreLoop.contains(kProgram.transition.begin));
static_assert(kProgram.gpuUpdate.contains(kProgram.firstVSync));
static_assert(kProgram.gpuUpdate.contains(kProgram.afterFirstVSync));
static_assert(kProgram.gpuUpdate.contains(kProgram.secondVSync));
static_assert(kProgram.gpuUpdate.contains(kProgram.afterVSync));

} // namespace

Crash1FrameDriver::Crash1FrameDriver(Game &game, Crash1FrameCut &frameCut) : game_(game), frameCut_(frameCut) {}

const crash::NativeFrameLoopContract &Crash1FrameDriver::contract() {
  return kContract;
}

const Crash1FrameProgram &Crash1FrameDriver::program() {
  return kProgram;
}

Crash1FrameDriver &Crash1FrameDriver::from(Core &core) {
  if (core.game == nullptr || core.game->frameDriver == nullptr) {
    lucent::error("crash1-frame", "Crash 1 frame override has no bound FrameDriver");
    std::abort();
  }
  return static_cast<Crash1FrameDriver &>(*core.game->frameDriver);
}

void Crash1FrameDriver::installOverrides(Game &game) {
  const struct Binding {
    std::uint32_t address;
    psx::cpu::NativeFunction function;
    const char *owner;
  } bindings[]{
      {kProgram.transition.begin, finishFrameIteration, "CoreLoop transition"},
      {kProgram.setRootCounter, setRootCounterSuper, "SetRCnt"},
      {kProgram.startRootCounter, startRootCounterSuper, "StartRCnt"},
      {kProgram.stopRootCounter, stopRootCounterSuper, "StopRCnt"},
  };
  for (const Binding &binding : bindings) {
    psx::cpu::installNativeOverride(game.core, binding.address, binding.owner, binding.function);
  }
}

void Crash1FrameDriver::callOriginal(Core &core, std::uint32_t address) {
  psx::cpu::callOriginalToReturn(core, address, psx::cpu::ExecutionBudget::currentTurn(core), "Crash 1 original call");
}

void Crash1FrameDriver::resetRootCounterClock(const Core &core) {
  nextRootCounterTick_ = core.game->timing.emulatedCpuTicks() + kRootCounterCpuTicks;
}

void Crash1FrameDriver::serviceRootCounter(Core &core, std::uint64_t throughCpuTick) {
  if (!rootCounterConfigured_ || !rootCounterRunning_) {
    return;
  }
  while (nextRootCounterTick_ <= throughCpuTick) {
    const R3000 interrupted = static_cast<const R3000 &>(core);
    psx::cpu::dispatchGuestToReturn(core,
                                    kProgram.rootCounterIncrement,
                                    psx::cpu::ExecutionBudget::currentTurn(core),
                                    "Crash 1 root-counter callback");
    static_cast<R3000 &>(core) = interrupted;
    nextRootCounterTick_ += kRootCounterCpuTicks;
  }
}

void Crash1FrameDriver::setRootCounterSuper(Core *core) {
  Crash1FrameDriver &driver = from(*core);
  const std::uint32_t spec = core->r[4];
  const std::uint32_t target = core->r[5];
  driver.callOriginal(*core, kProgram.setRootCounter);
  if (spec == kRootCounterSpec && target == kRootCounterTarget) {
    driver.rootCounterConfigured_ = true;
    driver.resetRootCounterClock(*core);
  }
}

void Crash1FrameDriver::startRootCounterSuper(Core *core) {
  Crash1FrameDriver &driver = from(*core);
  const std::uint32_t spec = core->r[4];
  driver.callOriginal(*core, kProgram.startRootCounter);
  if (spec == kRootCounterSpec) {
    if (!driver.rootCounterConfigured_) {
      lucent::error("crash1-frame", "StartRCnt(CNT2) preceded the measured SetRCnt(CNT2,0x1000) setup");
      std::abort();
    }
    driver.rootCounterRunning_ = true;
    driver.resetRootCounterClock(*core);
  }
}

void Crash1FrameDriver::stopRootCounterSuper(Core *core) {
  Crash1FrameDriver &driver = from(*core);
  const std::uint32_t spec = core->r[4];
  if (spec == kRootCounterSpec) {
    driver.serviceRootCounter(*core, core->game->timing.emulatedCpuTicks());
    driver.rootCounterRunning_ = false;
  }
  driver.callOriginal(*core, kProgram.stopRootCounter);
}

void Crash1FrameDriver::deliverDisplayField(Core &core) {
  if (!rootCounterConfigured_ || !rootCounterRunning_) {
    lucent::error("crash1-frame", "GpuUpdate reached its display wait before Crash 1 CNT2 was configured and started");
    std::abort();
  }
  if (deliveredFields_ >= 2) {
    lucent::error("crash1-frame", "one retail GpuUpdate requested more than its measured two display fields");
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

void Crash1FrameDriver::finishFrameIteration(Core *core) {
  Crash1FrameDriver &driver = from(*core);
  if (core->r[31] != kProgram.transition.begin) {
    lucent::error("crash1-frame",
                  "CoreLoop transition reached with ra=0x{:08X}; expected retail GpuUpdate return 0x{:08X}",
                  core->r[31],
                  kProgram.transition.begin);
    std::abort();
  }
  driver.frameCompleted_ = true;
  // requestExecutionExit stamps no PC; the address is provenance only, the driver resumes from $r31.
  psx::cpu::requestExecutionExit(
      *core,
      psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::FrameBoundary, kContract.guestVSync.begin, 0, {}});
}

psx::cpu::ExecutionResult
Crash1FrameDriver::runGuestToBoundary(Core &core, std::uint32_t entry, const ExecuteSlice &execute) {
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

void Crash1FrameDriver::stepFrame(Core &core, std::uint32_t frame) {
  if (&core != &game_.core || core.game != &game_) {
    lucent::error("crash1-frame", "Crash1FrameDriver was asked to step a different Game/Core");
    std::abort();
  }
  if (completedFrames_ != 0 && frame != lastHostFrame_ + 1) {
    lucent::error("crash1-frame", "non-sequential frame {} requested after host frame {}", frame, lastHostFrame_);
    std::abort();
  }
  lastHostFrame_ = frame;

  serviceRootCounter(core, game_.timing.emulatedCpuTicks());
  game_.timing.logicFrame = completedFrames_;
  game_.timing.frameTick();
  core.rsub.otAttr.beginLogicFrame(completedFrames_);
  game_.pad.serviceFrame();
  bios_pad_input::publishPrimary(core, game_.pad.buttons);
  deliveredFields_ = 0;
  frameCompleted_ = false;

  psx::cpu::ExecutionResult result;
  if (!enteredCoreLoop_) {
    core.r[4] = 25u;
    result = runGuestToBoundary(core, kProgram.coreLoop.begin, executeOneHostTurn);
    enteredCoreLoop_ = true;
  } else {
    result = runGuestToBoundary(core, kProgram.iteration.begin, executeOneHostTurn);
  }
  while (!frameCompleted_ && result.reason == psx::cpu::ExecutionExitReason::FrameBoundary) {
    if (!Crash1FrameDriver::isMeasuredFrameBoundary(result.guestPc,
                                                    core.r[31],
                                                    kContract.guestVSync.begin,
                                                    kProgram.afterFirstVSync,
                                                    kProgram.afterVSync,
                                                    kProgram.transition.begin)) {
      lucent::error("crash1-frame",
                    "frame {} reached an unexpected boundary at 0x{:08X} with ra=0x{:08X}; the measured "
                    "display waits are the guest's own VSync calls returning to 0x{:08X}/0x{:08X}, or "
                    "the transition override at 0x{:08X} stamping 0x{:08X}",
                    frame,
                    result.guestPc,
                    core.r[31],
                    kProgram.afterFirstVSync,
                    kProgram.afterVSync,
                    kProgram.transition.begin,
                    kContract.guestVSync.begin);
      std::abort();
    }
    const std::uint32_t continuation = core.r[31];
    deliverDisplayField(core);
    result = runGuestToBoundary(core, continuation, executeOneHostTurn);
  }
  if (!frameCompleted_ || result.reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    lucent::error("crash1-frame",
                  "frame {} left guest execution at 0x{:08X} with {} after {} cycles ({}); "
                  "expected the measured frame boundary",
                  frame,
                  result.guestPc,
                  psx::cpu::executionExitName(result.reason),
                  result.cycles,
                  result.detail);
    std::abort();
  }

  if (core.mem_r32(kProgram.doneAddress) != 0) {
    lucent::error("crash1-frame",
                  "Crash 1 requested CoreLoop exit after frame {}; its measured two-GpuUpdate drain/NSKill "
                  "transition is not yet extracted into host turns",
                  frame);
    std::abort();
  }
  if (deliveredFields_ == 0 || deliveredFields_ > 2) {
    lucent::error("crash1-frame", "frame {} returned without the measured one-or-two GpuUpdate display waits", frame);
    std::abort();
  }

  frameCut_.observe(core);
  game_.presentation.commit(&core, static_cast<int>(deliveredFields_), game_.temporalPresentation.get());
  ++completedFrames_;
}

} // namespace crash1
