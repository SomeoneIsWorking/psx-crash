// view_entry.h — what Gfx_UpdateView steps when unpaused, as each real call found it: the screen shake and the
// streaks' scroll. The replay's own call steps it again, so each in-between starts it from the blend of two entries.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstdint>

class Core;

namespace crash2 {

class ViewEntry final : public psx::present::MotionSource {
public:
  [[nodiscard]] const char *name() const override {
    return "view entry";
  }
  // Gfx_UpdateView is about to run for a real frame.
  void record(Core &core);
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  // g_ScreenShake, then the streaks' last and current scroll (0x80062A6C..0x80062A78).
  using Words = std::array<std::uint32_t, 5>;
  Words entered_{};
  Words from_{};
  Words to_{};
};

} // namespace crash2
