// draw_section.h — Crash 2's draw section: Sys_MainLoop's picture half, from Gfx_UpdateView to Gfx_EndFrame's band,
// with the objects queued by the game's own paused pass, Obj_UpdateAll(0).
#pragma once

#include "replay_frame.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace crash2 {

class DrawSection final : public psx::present::ReplaySection {
public:
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  // The two frame buffers' ordering tables, as the section last linked them; Gfx_AllocFrameBuffers places them per
  // level.
  mutable std::array<std::uint32_t, 2> heads_{};
};

} // namespace crash2
