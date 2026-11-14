// world_renderer.h — Crash 2's Gfx_DrawWorlds (0x80041E5C) facts over the shared world renderer
// (game/render/world_renderer.h): its scene modes, guest calls and retail instruction counts (crash2-decomp
// docs/notes/world-renderer.md).
#pragma once

class Core;

namespace crash2 {

class WorldRenderer {
public:
  static void registerOverrides(Core &core);
};

} // namespace crash2
