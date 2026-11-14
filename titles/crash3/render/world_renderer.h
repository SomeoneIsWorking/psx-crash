// world_renderer.h — Crash 3's world renderer (0x80040ED8) facts over the shared one (game/render/world_renderer.h):
// its scene modes, link tails, guest calls and retail instruction counts (crash3-decomp docs/notes/world-renderer.md).
#pragma once

class Core;

namespace crash3 {

class WorldRenderer {
public:
  static void registerOverrides(Core &core);
};

} // namespace crash3
