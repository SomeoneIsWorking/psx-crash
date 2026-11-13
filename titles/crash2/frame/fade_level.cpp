#include "crash2/frame/fade_level.h"

#include "blend.h"
#include "core.h"
#include "crash2/entry/session.h"
#include "native_dispatch.h"

namespace crash2 {
namespace {

// Names and layouts from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr std::uint32_t kDrawFade = 0x80016B08u;
constexpr std::uint32_t kBuildBuffer = 0x80063458u;
// GfxFrame's primitive cursor.
constexpr std::uint32_t kCursor = 0x08u;
// Gfx_DrawFade takes its draw mode, then its flat quad; the quad's grey is its r, g and b.
constexpr std::uint32_t kQuad = 0x0Cu;
constexpr std::uint32_t kGrey = 0x04u;
constexpr std::uint32_t kFrameFlags = 0x8006CB94u;
// Set: blend mode 1 (adds); clear: mode 2 (subtracts).
constexpr std::uint32_t kAdditive = 0x800000u;

bool subtractive(Core &core) {
  return (core.mem_r32(kFrameFlags) & kAdditive) == 0u;
}

} // namespace

void FadeLevel::sample(Core &core) {
  from_ = drawn_;
  fromSubtractive_ = drawnSubtractive_;
  drawn_.reset();
  subtractive_ = subtractive(core);
}

std::optional<FadeLevel::Quad> FadeLevel::earlier() const {
  if (from_ && fromSubtractive_ != subtractive_) {
    return std::nullopt;
  }
  return from_.value_or(Quad{});
}

psx::present::MotionCount FadeLevel::write(psx::present::GuestRedraw &, float) const {
  const bool unpaired = from_ && from_->grey != 0u && fromSubtractive_ != subtractive_;
  return {.drawn = from_ && from_->grey != 0u ? 1u : 0u, .unpaired = unpaired ? 1u : 0u};
}

void FadeLevel::draw(Core &core) {
  const std::optional<float> t = session(core).drawReplay.frame().replaying();
  const std::uint32_t later = core.r[4];
  const std::optional<Quad> from = t ? earlier() : std::nullopt;
  // A later tick that draws none still links the earlier tick's quad, to fade it out.
  const std::uint32_t level = later == 0u && from ? from->level : later;
  const std::uint32_t quad = core.mem_r32(core.mem_r32(kBuildBuffer) + kCursor) + kQuad;
  core.r[4] = level;
  psx::cpu::callOriginalToReturn(core, kDrawFade, psx::cpu::ExecutionBudget::currentTurn(core), "Crash 2 fade");
  if (level == 0u) {
    if (!t) {
      drawn_ = Quad{};
      drawnSubtractive_ = subtractive(core);
    }
    return;
  }
  const std::uint8_t grey = later == 0u ? 0u : core.mem_r8(quad + kGrey);
  if (!t) {
    drawn_ = Quad{.level = level, .grey = grey};
    drawnSubtractive_ = subtractive(core);
    return;
  }
  if (!from) {
    return;
  }
  const auto blended = static_cast<std::uint8_t>(psx::present::lerpInt(from->grey, grey, *t));
  for (std::uint32_t i = 0; i < 3u; ++i) {
    core.mem_w8(quad + kGrey + i, blended);
  }
}

void FadeLevel::drawOverride(Core *core) {
  session(*core).drawReplay.fade().draw(*core);
}

void FadeLevel::registerOverride(Core &core) {
  psx::cpu::installNativeOverride(core, kDrawFade, "Crash 2 fade level", drawOverride);
}

} // namespace crash2
