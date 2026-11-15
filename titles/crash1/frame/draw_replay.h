// draw_replay.h — Crash 1's in-betweens: the draw section replayed with the camera, the objects' placement and their
// vertex-animated poses blended.
#pragma once

#include "crash1/frame/draw_section.h"
#include "crash1/frame/pose_blend.h"
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
  [[nodiscard]] PoseBlend &poses() {
    return poses_;
  }
  [[nodiscard]] const PoseBlend &poses() const {
    return poses_;
  }

private:
  DrawSection section_;
  PoseBlend poses_;
  psx::present::ReplayFrame frame_;
};

} // namespace crash1
