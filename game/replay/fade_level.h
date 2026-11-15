// fade_level.h — the screen fade's grey in a replay, between the two ticks' Gfx_DrawFade quads.
//
// The end of frame steps its fade word before it draws it, so the replay's own call draws the later tick's grey
// (crash2-decomp docs/notes/frame-loop.md, Fade). Each real call's level, grey and blend mode are recorded; a replay's
// quad takes the grey t of the way from the earlier tick's to the later's, a tick that drew none counting as grey 0.
// Two ticks in different blend modes draw the later.
#pragma once

#include "motion_set.h"
#include "replay/replay_facts.h"

#include <cstdint>
#include <optional>

class Core;

namespace crash {

class FadeLevel final : public psx::present::MotionSource {
public:
  explicit FadeLevel(const ReplayFacts &facts) : facts_(facts) {}

  [[nodiscard]] const char *name() const override {
    return "fade";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;
  // Gfx_DrawFade's call, from its override.
  void draw(Core &core, std::optional<float> t);

private:
  struct Quad {
    std::uint32_t level = 0;
    std::uint8_t grey = 0;
  };
  [[nodiscard]] bool subtractive(Core &core) const;
  // The earlier tick's quad when it pairs with the later tick's blend mode.
  [[nodiscard]] std::optional<Quad> earlier() const;

  const ReplayFacts &facts_;
  // The quad the last real end of frame drew since the sample, and its blend mode.
  std::optional<Quad> drawn_;
  bool drawnSubtractive_ = false;
  // The earlier tick's, and the later tick's blend mode.
  std::optional<Quad> from_;
  bool fromSubtractive_ = false;
  bool subtractive_ = false;
};

} // namespace crash
