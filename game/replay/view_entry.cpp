#include "replay/view_entry.h"

#include "blend.h"
#include "core.h"
#include "guest_redraw.h"

namespace crash {
namespace {

// The streak words: two u16 scrolls, a z, then the same three as they step.
constexpr std::array<bool, 4> kStreakHalves{true, false, true, false};

} // namespace

void ViewEntry::record(Core &core) {
  entered_[0] = core.mem_r32(facts_.screenShake);
  if (facts_.streakScroll) {
    for (std::uint32_t i = 0; i < kStreakHalves.size(); ++i) {
      entered_[i + 1] = core.mem_r32(*facts_.streakScroll + 4u * i);
    }
  }
}

void ViewEntry::sample(Core &) {
  from_ = to_;
  to_ = entered_;
}

psx::present::MotionCount ViewEntry::write(psx::present::GuestRedraw &redraw, float t) const {
  // The shake alternates sign each tick; the in-between shows the later tick's.
  redraw.writeWord(facts_.screenShake, to_[0]);
  if (facts_.streakScroll) {
    for (std::size_t i = 0; i < kStreakHalves.size(); ++i) {
      const std::uint32_t from = from_[i + 1];
      const std::uint32_t to = to_[i + 1];
      const std::uint32_t word = kStreakHalves[i] ? psx::present::lerpTurn(from & 0xFFFFu, to & 0xFFFFu, 16, t) |
                                                        psx::present::lerpTurn(from >> 16, to >> 16, 16, t) << 16
                                                  : psx::present::lerpWord(from, to, t);
      redraw.writeWord(*facts_.streakScroll + 4u * static_cast<std::uint32_t>(i), word);
    }
  }
  return {.drawn = 1, .unpaired = 0};
}

} // namespace crash
