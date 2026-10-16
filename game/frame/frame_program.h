#pragma once

#include "runtime/guest_function_range.h"

#include <cstdint>
#include <optional>

namespace crash {

// A title's scene table in guest RAM: `rows` rows of `rowBytes`, indexed by scene id; word 1 of a row masked by
// `sizeMask` is the scene header's byte size, zero for an id the title never loads.
struct SceneTable {
  std::uint32_t address;
  std::uint32_t rows;
  std::uint32_t rowBytes;
  std::uint32_t sizeMask;
};

// GpuUpdate's frame limiter: it skips both display waits once the VBlank counter its VSync callback bumps
// is `skipAfterFields` past the stamp in the displayed frame record (`*displayedFrame + stampOffset`).
struct FrameLimiter {
  std::uint32_t vblankCounter;
  std::uint32_t displayedFrame;
  std::uint32_t stampOffset;
  std::uint32_t skipAfterFields;
};

// A buffer swap outside GpuUpdate whose own VSync(0) is a display wait: Crash 1's scene loader shows its loading
// picture through one, twice per load.
struct DisplaySwap {
  GuestFunctionRange body;
  std::uint32_t afterVSync;
};

// One title's measured frame loop: where a host frame starts and ends, and the display waits between.
struct FrameProgram {
  GuestFunctionRange coreLoop;
  // CoreLoop's argument on its first entry: the scene it starts in.
  std::uint32_t initialScene;
  // Each host frame after the first resumes at `iteration.begin`.
  GuestFunctionRange iteration;
  // The override that ends a host frame, reached by the guest returning to `transitionReturn`: an empty
  // stub call (Crash 2) or the GpuUpdate return inside CoreLoop (Crash 1 and 3).
  GuestFunctionRange transition;
  std::uint32_t transitionReturn;
  GuestFunctionRange gpuUpdate;
  std::uint32_t firstVSync;
  std::uint32_t afterFirstVSync;
  std::uint32_t secondVSync;
  std::uint32_t afterVSync;
  // Empty when GpuUpdate always reaches at least one display wait.
  std::optional<FrameLimiter> limiter;
  // Empty when every display wait is inside GpuUpdate.
  std::optional<DisplaySwap> loadingSwap;
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
