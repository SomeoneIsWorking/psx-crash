// draw_replay.h — Crash 2's in-betweens: the draw section replayed with the camera, the object pool's poses and the
// view's stepped state blended.
#pragma once

#include "crash2/frame/draw_section.h"
#include "crash2/frame/view_entry.h"
#include "replay_frame.h"

#include <array>
#include <cstddef>

class Core;

namespace crash2 {

class DrawReplay {
public:
  DrawReplay();

  // The sample point, Sys_BeforeDrawObjects (the empty function Sys_MainLoop calls between Obj_UpdateAll and
  // Gfx_DrawObjects), and the view entry's record at Gfx_UpdateView.
  static void registerOverrides(Core &core);

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }
  [[nodiscard]] ViewEntry &view() {
    return view_;
  }
  // The object pool's track, and its animation fields' places in a sample.
  [[nodiscard]] static const psx::present::MotionTrack &objectTrack();
  static constexpr std::size_t kAnimField = 9;
  static constexpr std::size_t kAnimFrameField = 10;

private:
  ViewEntry view_;
  DrawSection section_;
  std::array<psx::present::MotionSource *, 1> sources_{&view_};
  psx::present::ReplayFrame frame_;
};

} // namespace crash2
