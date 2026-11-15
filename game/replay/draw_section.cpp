#include "replay/draw_section.h"

#include "core.h"
#include "guest_redraw.h"

#include <algorithm>
#include <span>

namespace crash {

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  const ReplayFacts &f = facts_;
  // That pass runs Obj_UpdateAll before the world and no view; the section starts at the view call.
  if ((core.mem_r32(f.frameFlags) & f.objectsFirst) != 0u) {
    return std::nullopt;
  }
  const std::uint32_t frame = core.mem_r32(f.buildBuffer);
  // The buffer as the last end of frame left it: primitives from the start, no object queued.
  const std::array<std::uint32_t, 1> buffer{frame};
  // The real frame's Obj_UpdateAll argument, so the same objects queue; with logic off each one only places itself.
  const std::array<std::uint32_t, 1> unpaused{core.mem_r32(f.paused) == 0u ? 1u : 0u};
  const std::array<std::uint32_t, 1> endArgument{f.endFrameArgument.value_or(0u)};
  const std::span<const std::uint32_t> endArguments =
      f.endFrameArgument ? std::span<const std::uint32_t>(endArgument) : std::span<const std::uint32_t>{};
  if (!redraw.dispatch(f.resetFrame, buffer) || !redraw.resumeTo(f.mainLoop, f.viewCall, f.objectsStop)) {
    return std::nullopt;
  }
  redraw.writeWord(f.frameFlags, core.mem_r32(f.frameFlags) & ~f.objectLogic);
  if (!redraw.dispatch(f.objUpdateAll, unpaused) || !redraw.resumeTo(f.mainLoop, f.drawObjects, f.endFrameStop) ||
      !redraw.dispatchTo(f.endFrame, f.endFrameDeviceTail, endArguments)) {
    return std::nullopt;
  }
  const std::uint32_t head = frame + f.orderingTable;
  if (std::ranges::find(heads_, head) == heads_.end()) {
    heads_ = {heads_[1], head};
  }
  return head;
}

bool DrawSection::linked(std::uint32_t head) const {
  return head != 0u && std::ranges::find(heads_, head) != heads_.end();
}

} // namespace crash
