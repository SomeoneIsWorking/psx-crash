// screen_effects.h — the screen shake and the fade overlay in a Crash 1 in-between. A replay runs after the newest
// tick's Gfx_UpdateMatrices has stepped the shake for the tick after, so it draws the newest tick's shake again; it
// steps the fade to the newest tick's value itself, which is drawn t of the way from the tick before's.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace crash1 {

class ScreenEffects {
public:
  // The real object pass starts: the fade the last real Gpu_Update drew becomes the tick before's.
  void openPass();
  // Gfx_UpdateMatrices and Gpu_DrawOverlay, from DrawReplay.
  static void registerOverrides(Core &core);
  // Run end: fades drawn as the newest tick because the tick before drew none.
  void report() const;

private:
  static void matricesOverride(Core *core);
  static void overlayOverride(Core *core);
  void matrices(Core &core, bool replaying);
  void overlay(Core &core, std::optional<float> t);

  std::uint32_t shake_ = 0;
  std::optional<std::int32_t> drawnFade_;
  std::optional<std::int32_t> drawingFade_;
  std::uint64_t unpairedFades_ = 0;
};

} // namespace crash1
