#include "crash1/frame/draw_section.h"

#include "core.h"
#include "crash1/frame/frame_facts.h"
#include "guest_redraw.h"

#include <algorithm>

namespace crash1 {

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  using namespace frame_facts;
  // The buffer as the last Gpu_Update left it for this frame: its ordering table cleared, its primitives from the base.
  const std::uint32_t buffer = core.mem_r32(kOtCur);
  const std::uint32_t orderingTable = buffer + kOrderingTable;
  redraw.writeWord(buffer + kPrimCursor, core.mem_r32(buffer + kPrimBase));
  const std::array<std::uint32_t, 2> clear{orderingTable, kOrderingTableEntries};
  if (!redraw.dispatch(kGpuRResetOt, clear)) {
    return std::nullopt;
  }
  redraw.writeWord(kFrameFlags, core.mem_r32(kFrameFlags) & ~kObjectLogic);
  if (!redraw.resumeTo(kCoreLoop, kMatricesCall, kGpuUpdateCall) ||
      !redraw.dispatchTo(kGpuUpdate, kAudioUpdateCall, {})) {
    return std::nullopt;
  }
  if (std::ranges::find(heads_, orderingTable) == heads_.end()) {
    heads_ = {heads_[1], orderingTable};
  }
  return orderingTable;
}

bool DrawSection::linked(std::uint32_t head) const {
  return head != 0u && std::ranges::find(heads_, head) != heads_.end();
}

} // namespace crash1
