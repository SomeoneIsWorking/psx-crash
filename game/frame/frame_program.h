#pragma once

#include "native_frame_loop_contract.h"

#include <cstdint>

namespace crash {

// One title's measured frame loop: where a host frame starts and ends, and the display waits between.
struct FrameProgram {
  GuestFunctionRange coreLoop;
  // CoreLoop's argument on its first entry: the scene it starts in.
  std::uint32_t initialScene;
  // The loop body: each host frame after the first resumes at its entry.
  GuestFunctionRange iteration;
  // The override that ends a host frame, reached by the guest returning to `transitionReturn`.
  GuestFunctionRange transition;
  std::uint32_t transitionReturn;
  GuestFunctionRange gpuUpdate;
  std::uint32_t firstVSync;
  std::uint32_t afterFirstVSync;
  std::uint32_t secondVSync;
  std::uint32_t afterVSync;
  std::uint32_t doneAddress;
  std::uint32_t sceneIdAddress;
  std::uint32_t sceneRequestAddress;
  std::uint32_t rootCounterIncrement;
  std::uint32_t setRootCounter;
  std::uint32_t startRootCounter;
  std::uint32_t stopRootCounter;
};

} // namespace crash
