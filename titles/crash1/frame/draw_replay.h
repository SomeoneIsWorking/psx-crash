// draw_replay.h — Crash 1's in-betweens: the draw section replayed with the camera and the objects' placement blended.
#pragma once

#include "crash1/frame/draw_section.h"
#include "replay_frame.h"

class Core;

namespace crash1 {

class DrawReplay {
public:
  DrawReplay();

  // The sample point, Gool_UpdateObjects' return to CORE_Loop once the frame's object logic has run, and Pad_Update,
  // which a replay skips.
  static void registerOverrides(Core &core);

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }

private:
  DrawSection section_;
  psx::present::ReplayFrame frame_;
};

} // namespace crash1
