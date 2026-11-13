#include "crash2/frame/draw_section.h"

#include "core.h"
#include "guest_redraw.h"

#include <algorithm>

namespace crash2 {
namespace {

// Names and sites from external/crash2-decomp (config/symbol_addrs.txt, docs/our-names.md, docs/notes/frame-loop.md).
constexpr std::uint32_t kSysMainLoop = 0x80011800u;
// Sys_MainLoop + 0x368: the jal Gfx_UpdateView after Cam_Update, the picture half's first block.
constexpr std::uint32_t kViewCall = 0x80011B68u;
// Sys_MainLoop + 0x580: the Obj_UpdateAll argument load, after the world, streaks and func_80045218.
constexpr std::uint32_t kObjectsCall = 0x80011D80u;
// Sys_MainLoop + 0x594: past the Sys_BeforeDrawObjects call, the Gfx_DrawObjects argument loads.
constexpr std::uint32_t kDrawObjects = 0x80011D94u;
// Sys_MainLoop + 0x5B4: the jal Gfx_EndFrame.
constexpr std::uint32_t kEndFrameCall = 0x80011DB4u;
constexpr std::uint32_t kObjUpdateAll = 0x8001C29Cu;
constexpr std::uint32_t kGfxResetFrame = 0x800163C4u;
constexpr std::uint32_t kGfxEndFrame = 0x8001645Cu;
// Gfx_EndFrame + 0x334: the jal Snd_Update, after the fade and the band; the swap and DrawOTag follow.
constexpr std::uint32_t kSoundUpdate = 0x80016790u;
constexpr std::uint32_t kBuildBuffer = 0x80063458u;
// GfxFrame.ot, which Gfx_EndFrame hands DrawOTag.
constexpr std::uint32_t kOrderingTableOffset = 0x18u;

} // namespace

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  const std::uint32_t frame = core.mem_r32(kBuildBuffer);
  // The buffer as the last Gfx_EndFrame left it for this frame: primitives from the start, no object queued.
  const std::array<std::uint32_t, 1> buffer{frame};
  const std::array<std::uint32_t, 1> paused{0u};
  if (!redraw.dispatch(kGfxResetFrame, buffer) || !redraw.resumeTo(kSysMainLoop, kViewCall, kObjectsCall) ||
      !redraw.dispatch(kObjUpdateAll, paused) || !redraw.resumeTo(kSysMainLoop, kDrawObjects, kEndFrameCall) ||
      !redraw.dispatchTo(kGfxEndFrame, kSoundUpdate, {})) {
    return std::nullopt;
  }
  const std::uint32_t head = frame + kOrderingTableOffset;
  if (std::ranges::find(heads_, head) == heads_.end()) {
    heads_ = {heads_[1], head};
  }
  return head;
}

bool DrawSection::linked(std::uint32_t head) const {
  return head != 0u && std::ranges::find(heads_, head) != heads_.end();
}

} // namespace crash2
