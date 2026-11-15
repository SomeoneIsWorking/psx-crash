// draw_section.h — a Crash 2 or Crash 3 draw section: Sys_MainLoop's picture half, from Gfx_UpdateView to the end of
// frame's draws, with Obj_UpdateAll queueing the objects the real walk queued (ObjectWalk) while g_FrameFlags runs no
// object logic.
#pragma once

#include "replay/replay_facts.h"
#include "replay_frame.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace crash {

class DrawSection final : public psx::present::ReplaySection {
public:
  explicit DrawSection(const ReplayFacts &facts) : facts_(facts) {}

  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  const ReplayFacts &facts_;
  // The two frame buffers' ordering tables, as the section last linked them; they are placed per level.
  mutable std::array<std::uint32_t, 2> heads_{};
};

} // namespace crash
