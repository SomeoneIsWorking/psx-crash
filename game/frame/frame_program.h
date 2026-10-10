#pragma once

#include "native_frame_loop_contract.h"

#include <cstdint>

namespace crash {

// A title's scene table in guest RAM: `rows` rows of `rowBytes`, indexed by scene id; word 1 of a row masked by
// `sizeMask` is the scene header's byte size, zero for an id the title never loads.
struct SceneTable {
  std::uint32_t address;
  std::uint32_t rows;
  std::uint32_t rowBytes;
  std::uint32_t sizeMask;
};

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
  // Zero address when the title has no measured table.
  SceneTable sceneTable;
  std::uint32_t rootCounterIncrement;
  std::uint32_t setRootCounter;
  std::uint32_t startRootCounter;
  std::uint32_t stopRootCounter;
};

} // namespace crash
