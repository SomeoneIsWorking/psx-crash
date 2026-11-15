// draw_section.h — Crash 1's draw section: CORE_Loop's picture half, from Gfx_UpdateMatrices through the worlds and
// Gool_UpdateObjects to Gpu_Update's fade and backdrop, run with g_cur_lid_ro's object logic bit clear so each object
// only transforms itself.
#pragma once

#include "replay_frame.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace crash1 {

class DrawSection final : public psx::present::ReplaySection {
public:
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  // The two render buffers' ordering tables, as the section last linked them.
  mutable std::array<std::uint32_t, 2> heads_{};
};

} // namespace crash1
