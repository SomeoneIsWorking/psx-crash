#pragma once

#include "execution_exit.h"
#include "frame_cut.h"
#include "frame_program.h"
#include "game_runtime.h"
#include "native_frame_loop_contract.h"
#include "scene_warp.h"

#include <cstdint>
#include <functional>

class Game;

namespace crash {

// One host frame of a Crash title: guest turns to the measured display waits and the loop's transition,
// one display field per wait, then the presentation commit.
class CrashFrameDriver : public FrameDriver {
public:
  using ExecuteSlice = std::function<psx::cpu::ExecutionResult(Core &, std::uint32_t)>;

  CrashFrameDriver(Game &game,
                   FrameCut &frameCut,
                   const FrameProgram &program,
                   const NativeFrameLoopContract &contract);

  void stepFrame(Core &core, std::uint32_t frame) override;
  psx::cpu::ExecutionResult runGuestToBoundary(Core &core, std::uint32_t entry, const ExecuteSlice &execute);

  static void installOverrides(Game &game, const FrameProgram &program);

  // Arms a `warp <scene>` request; the next frame raises it through the guest's own scene request.
  std::string armWarp(Core &core, const char *line);

  // Two measured sources: the guest's `jal` to the libetc VSync leaf in GpuUpdate, and the loop
  // transition override (which stamps the VSync leaf entry). Anything else is refused.
  [[nodiscard]] static constexpr bool isMeasuredFrameBoundary(std::uint32_t guestPc,
                                                              std::uint32_t returnAddress,
                                                              std::uint32_t vsyncLeaf,
                                                              std::uint32_t afterFirst,
                                                              std::uint32_t afterSecond,
                                                              std::uint32_t transitionReturn) noexcept {
    const bool fromGuestVsync = (guestPc == afterFirst && returnAddress == afterFirst) ||
                                (guestPc == afterSecond && returnAddress == afterSecond);
    const bool fromTransitionOverride = guestPc == vsyncLeaf && returnAddress == transitionReturn;
    return fromGuestVsync || fromTransitionOverride;
  }

protected:
  // Publishes the host pad where the title reads it, before the frame's guest turn. The framework fills
  // buffers the title declares through `guestPadBufferLayout`; a title that reads a word of its own overrides this.
  virtual void publishInput(Core &core);

  Game &game() {
    return game_;
  }

private:
  static void finishFrameIteration(Core *core);
  static void setRootCounterSuper(Core *core);
  static void startRootCounterSuper(Core *core);
  static void stopRootCounterSuper(Core *core);
  static CrashFrameDriver &from(Core &core);

  void deliverDisplayField(Core &core);
  void serviceRootCounter(Core &core, std::uint64_t throughCpuTick);
  void resetRootCounterClock(const Core &core);
  void callOriginal(Core &core, std::uint32_t address);

  Game &game_;
  FrameCut &frameCut_;
  const FrameProgram &program_;
  const NativeFrameLoopContract &contract_;
  std::uint32_t completedFrames_{};
  // The host numbers steps from its own base (the title host starts at 1), so only consecutiveness is checked.
  std::uint32_t lastHostFrame_{};
  std::uint32_t deliveredFields_{};
  std::uint64_t waitBaseCpuTick_{};
  std::uint64_t nextRootCounterTick_{};
  bool enteredCoreLoop_{};
  bool rootCounterConfigured_{};
  bool rootCounterRunning_{};
  bool frameCompleted_{};
  SceneWarp warp_;
};

} // namespace crash
