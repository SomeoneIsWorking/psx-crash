#pragma once

#include "execution_exit.h"
#include "frame/frame_cut.h"
#include "frame/frame_program.h"
#include "frame/scene_warp.h"
#include "game_runtime.h"

#include <cstdint>
#include <functional>

class Game;

namespace crash {

// One host frame of a Crash title: guest turns to the measured display waits and the loop's transition,
// one display field per wait, then the presentation commit.
class FrameDriver : public psx::game::FrameDriver {
public:
  using ExecuteSlice = std::function<psx::cpu::ExecutionResult(Core &, std::uint32_t)>;

  // `execute` runs one guest turn; the product passes `executeHostTurn`.
  FrameDriver(
      Game &game, FrameCut &frameCut, const FrameProgram &program, std::uint32_t vsyncEntry, ExecuteSlice execute);

  // Runs the guest from `address` until a typed boundary or the turn's budget ends.
  static psx::cpu::ExecutionResult executeHostTurn(Core &core, std::uint32_t address);

  void stepFrame(Core &core, std::uint32_t frame) override;
  psx::cpu::ExecutionResult runGuestToBoundary(Core &core, std::uint32_t entry, const ExecuteSlice &execute);

  static void installOverrides(Game &game, const FrameProgram &program);

  // Arms a `warp <scene>` request; the next frame raises it through the guest's own scene request.
  std::string armWarp(Core &core, const psx::debug::CommandArgs &args);

  // What a guest frame boundary is: a VSync call in GpuUpdate or in the loading swap, or the loop transition
  // override (which stamps the VSync leaf entry). `Unmeasured` is refused.
  enum class Boundary : std::uint8_t { Unmeasured, GpuUpdateWait, LoadingSwapWait, Transition };

  [[nodiscard]] static constexpr Boundary classifyBoundary(std::uint32_t guestPc,
                                                           std::uint32_t returnAddress,
                                                           std::uint32_t vsyncLeaf,
                                                           const FrameProgram &program) noexcept {
    const auto returnsTo = [&](std::uint32_t site) {
      return guestPc == site && returnAddress == site;
    };
    if (returnsTo(program.afterFirstVSync) || returnsTo(program.afterVSync)) {
      return Boundary::GpuUpdateWait;
    }
    if (program.loadingSwap && returnsTo(program.loadingSwap->afterVSync)) {
      return Boundary::LoadingSwapWait;
    }
    if (guestPc == vsyncLeaf && returnAddress == program.transitionReturn) {
      return Boundary::Transition;
    }
    return Boundary::Unmeasured;
  }

protected:
  // Publishes the host pad where the title reads it, before the frame's guest turn. libpad buffers are
  // filled through `Runtime::deliverPad`; a title that reads a word of its own overrides this.
  virtual void publishInput(Core &core);

  Game &game() {
    return game_;
  }

private:
  static void finishFrameIteration(Core *core);
  static void setRootCounterSuper(Core *core);
  static void startRootCounterSuper(Core *core);
  static void stopRootCounterSuper(Core *core);

  void deliverField(Core &core, std::uint64_t boundaryCpuTick);
  void deliverDisplayWait(Core &core, Boundary wait);
  [[nodiscard]] bool limiterSkippedWaits(Core &core) const;
  void deliverSkippedWaits(Core &core, std::uint32_t frame);
  void serviceRootCounter(Core &core, std::uint64_t throughCpuTick);
  void resetRootCounterClock(const Core &core);
  void callOriginal(Core &core, std::uint32_t address);

  Game &game_;
  FrameCut &frameCut_;
  const FrameProgram &program_;
  std::uint32_t vsyncEntry_;
  ExecuteSlice execute_;
  std::uint32_t completedFrames_{};
  // The host numbers steps from its own base (the title host starts at 1), so only consecutiveness is checked.
  std::uint32_t lastHostFrame_{};
  // Display waits one source requested this frame, timed from its first wait.
  struct WaitGroup {
    std::uint32_t fields{};
    std::uint64_t baseCpuTick{};
  };

  std::uint32_t deliveredFields_{};
  WaitGroup gpuUpdateWaits_;
  WaitGroup loadingSwapWaits_;
  // Emulated time after the last commit: the display boundary the next frame's fields count from.
  std::uint64_t displayBoundaryTick_;
  std::uint64_t nextRootCounterTick_{};
  // CPU ticks between CNT2 targets, from the title's SetRCnt.
  std::uint64_t rootCounterPeriod_{};
  bool enteredCoreLoop_{};
  bool rootCounterConfigured_{};
  bool rootCounterRunning_{};
  bool frameCompleted_{};
  SceneWarp warp_;
};

} // namespace crash
