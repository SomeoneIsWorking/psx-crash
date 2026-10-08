#pragma once

#include <cstdint>
#include <string_view>

struct PlatformHlePlan;
class Game;

namespace crash {

struct GuestFunctionRange {
  std::uint32_t begin;
  std::uint32_t end;

  [[nodiscard]] constexpr bool contains(std::uint32_t address) const {
    return address >= begin && address < end;
  }

  [[nodiscard]] constexpr bool valid() const {
    return begin != 0 && begin < end;
  }
};

enum class NativeFrameLoopState {
  Ready,
  FiniteBootSeamOnly,
  Missing,
};

struct NativeFrameLoopContract {
  std::string_view codeword;
  GuestFunctionRange guestVSync;
  // Field counter the title's VSync body returns for a negative argument (a query, not a frame wait).
  // Crash 1 0x800549F0, Crash 2 0x8005DC98, Crash 3 0x8005F384. Zero means undeclared.
  std::uint32_t vsyncQueryCounter;
  NativeFrameLoopState state;
  std::string_view refusal;

  [[nodiscard]] constexpr bool canStepFrame() const {
    return state == NativeFrameLoopState::Ready;
  }
};

// Reached when a host frame is requested beyond the recorded RE frontier.
[[noreturn]] void abortUnprovenFrameStep(const NativeFrameLoopContract &contract, std::uint32_t frame);

[[nodiscard]] PlatformHlePlan makeNativeFramePlatformPlan(const NativeFrameLoopContract &contract);

// Installs and verifies the framework's typed VSync boundary before guest execution on direct routes.
void initializeNativeFrameLoopContract(Game &game);

} // namespace crash
