// view_entry.h — what Gfx_UpdateView steps when unpaused, as each real call found it: the screen shake and, where the
// title scrolls streaks there, their scroll. The replay's own call steps it again, so each in-between starts it from
// the blend of two entries.
#pragma once

#include "motion_set.h"
#include "replay/replay_facts.h"

#include <array>
#include <cstdint>

class Core;

namespace crash {

class ViewEntry final : public psx::present::MotionSource {
public:
  explicit ViewEntry(const ReplayFacts &facts) : facts_(facts) {}

  [[nodiscard]] const char *name() const override {
    return "view entry";
  }
  // Gfx_UpdateView is about to run for a real frame.
  void record(Core &core);
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  // The screen shake, then the streaks' last and current scroll.
  using Words = std::array<std::uint32_t, 5>;
  const ReplayFacts &facts_;
  Words entered_{};
  Words from_{};
  Words to_{};
};

} // namespace crash
