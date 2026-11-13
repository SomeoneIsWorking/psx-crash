#include "crash2/frame/view_entry.h"

#include "blend.h"
#include "core.h"

namespace crash2 {
namespace {

// Names from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr std::uint32_t kScreenShake = 0x8006CB78u;
constexpr std::uint32_t kStreakScroll = 0x80062A6Cu;
// The streak words: two u16 scrolls, a z, then the same three as they step.
constexpr std::array<bool, 4> kStreakHalves{true, false, true, false};

} // namespace

void ViewEntry::record(Core &core) {
  entered_[0] = core.mem_r32(kScreenShake);
  for (std::uint32_t i = 0; i < kStreakHalves.size(); ++i) {
    entered_[i + 1] = core.mem_r32(kStreakScroll + 4u * i);
  }
}

void ViewEntry::sample(Core &) {
  from_ = to_;
  to_ = entered_;
}

psx::present::MotionCount ViewEntry::write(psx::present::GuestRedraw &redraw, float t) const {
  // The shake alternates sign each tick; the in-between shows the later tick's.
  redraw.writeWord(kScreenShake, to_[0]);
  for (std::size_t i = 0; i < kStreakHalves.size(); ++i) {
    const std::uint32_t from = from_[i + 1];
    const std::uint32_t to = to_[i + 1];
    const std::uint32_t word = kStreakHalves[i] ? psx::present::lerpTurn(from & 0xFFFFu, to & 0xFFFFu, 16, t) |
                                                      psx::present::lerpTurn(from >> 16, to >> 16, 16, t) << 16
                                                : psx::present::lerpWord(from, to, t);
    redraw.writeWord(kStreakScroll + 4u * static_cast<std::uint32_t>(i), word);
  }
  return {.drawn = 1, .unpaired = 0};
}

} // namespace crash2
