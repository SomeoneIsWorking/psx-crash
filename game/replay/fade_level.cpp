#include "replay/fade_level.h"

#include "blend.h"
#include "core.h"
#include "native_dispatch.h"

namespace crash {
namespace {

// GfxFrame's primitive cursor; Gfx_DrawFade takes its draw mode there, then its flat quad, whose grey is its r, g, b.
constexpr std::uint32_t kCursor = 0x08u;
constexpr std::uint32_t kQuad = 0x0Cu;
constexpr std::uint32_t kGrey = 0x04u;

} // namespace

bool FadeLevel::subtractive(Core &core) const {
  return (core.mem_r32(facts_.frameFlags) & facts_.additiveFade) == 0u;
}

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

void FadeLevel::draw(Core &core, std::optional<float> t) {
  const std::uint32_t later = core.r[4];
  const std::optional<Quad> from = t ? earlier() : std::nullopt;
  // A later tick that draws none still links the earlier tick's quad, to fade it out.
  const std::uint32_t level = later == 0u && from ? from->level : later;
  const std::uint32_t quad = core.mem_r32(core.mem_r32(facts_.buildBuffer) + kCursor) + kQuad;
  core.r[4] = level;
  psx::cpu::callOriginalToReturn(core, facts_.drawFade, psx::cpu::ExecutionBudget::currentTurn(core), "Crash fade");
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

} // namespace crash
