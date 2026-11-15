// draw_replay.h — a Crash 2 or Crash 3 in-between: the draw section replayed with the camera, the object pool's and the
// player's placement, the view's stepped state and the screen fade blended; poses are the title's (QueuePoses).
#pragma once

#include "replay/draw_section.h"
#include "replay/fade_level.h"
#include "replay/object_walk.h"
#include "replay/replay_facts.h"
#include "replay/replay_tracks.h"
#include "replay/view_entry.h"
#include "replay_frame.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace crash {

class DrawReplay {
public:
  explicit DrawReplay(const ReplayFacts &facts);

  // The sample point (Sys_BeforeDrawObjects), the view entry's record at Gfx_UpdateView, Gfx_DrawFade, the object walk
  // and the queue.
  static void registerOverrides(Core &core, const ReplayFacts &facts);

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }
  [[nodiscard]] ObjectWalk &walk() {
    return walk_;
  }
  // An object's two samples, from the pool's track or the player's (ReplayTracks::kAnimField and kAnimFrameField name
  // its animation fields in them).
  [[nodiscard]] std::optional<psx::present::MotionRule::Samples> pairedObject(std::uint32_t object) const;

private:
  static void sampleOverride(Core *core);
  static void updateViewOverride(Core *core);
  static void drawFadeOverride(Core *core);

  const ReplayFacts &facts_;
  ReplayTracks tracks_;
  ViewEntry view_;
  FadeLevel fade_;
  ObjectWalk walk_;
  DrawSection section_;
  std::array<psx::present::MotionSource *, 2> sources_{&view_, &fade_};
  psx::present::ReplayFrame frame_;
};

} // namespace crash
