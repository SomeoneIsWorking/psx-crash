#include "render/world_screen.h"

#include "core.h"

namespace crash::world {
namespace {

inline constexpr std::uint32_t kLaneSign = 0x8000u;
inline constexpr std::uint32_t kTagLengthShift = 24u;
inline constexpr std::uint32_t kLinkMask = 0x00FFFFFFu;

} // namespace

ScreenVerdict ScreenTest::test(const std::uint32_t *screens, int corners) const {
  // Rows: the packed test's high lane (the low lane's borrow included), against the retail bound alone.
  std::uint32_t past = 0;
  std::uint32_t all = ~0u;
  // Columns: every corner right of the last column, or every corner left of the first.
  std::uint32_t right = 0;
  std::uint32_t left = kLaneSign;
  std::uint32_t retailRight = 0;
  std::uint32_t retailLeft = kLaneSign;
  const auto lastColumn = static_cast<std::uint32_t>(columns_.right);
  const auto firstColumn = static_cast<std::uint32_t>(columns_.left);
  for (int corner = 0; corner < corners; ++corner) {
    const std::uint32_t point = screens[corner];
    past |= point - kScreenEnd;
    all &= point;
    right |= (point - lastColumn) & kLaneSign;
    left &= (point - firstColumn) & kLaneSign;
    retailRight |= (point - static_cast<std::uint32_t>(kScreenColumns)) & kLaneSign;
    retailLeft &= point & kLaneSign;
  }
  if (static_cast<std::int32_t>(~past | all) < 0) {
    return {.cull = Cull::Rows};
  }
  if (right == 0u || left != 0u) {
    return {.cull = Cull::Columns};
  }
  return {.cull = Cull::Visible, .margin = retailRight == 0u || retailLeft != 0u};
}

void linkAtHead(Core &core, std::uint32_t slot, std::uint32_t &next, std::uint32_t words) {
  core.mem_w32(next, core.mem_r32(slot) | ((words - 1u) << kTagLengthShift));
  core.mem_w32(slot, next & kLinkMask);
  next += words * 4u;
}

} // namespace crash::world
