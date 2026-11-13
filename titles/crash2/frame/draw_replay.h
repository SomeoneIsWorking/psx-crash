// draw_replay.h — Crash 2's in-betweens: the draw section replayed with the camera and the object pool's poses blended.
#pragma once

#include "crash2/frame/draw_section.h"
#include "replay_frame.h"

class Core;

namespace crash2 {

class DrawReplay {
public:
  DrawReplay();

  // Installs the sample point: Sys_BeforeDrawObjects, the empty function Sys_MainLoop calls between Obj_UpdateAll and
  // Gfx_DrawObjects.
  static void registerOverride(Core &core);

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }

private:
  DrawSection section_;
  psx::present::ReplayFrame frame_;
};

} // namespace crash2
