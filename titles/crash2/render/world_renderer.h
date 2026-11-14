// world_renderer.h — Gfx_DrawWorlds (0x80041E5C) in host code: the sort list's polygons projected, culled against the
// presented canvas's columns and linked into the frame's ordering table as the retail code links them
// (crash2-decomp docs/notes/world-renderer.md). At 4:3 its memory, GTE and packets are the retail routine's.
#pragma once

class Core;

namespace crash2 {

class WorldRenderer {
public:
  static void registerOverrides(Core &core);

private:
  static void draw(Core *core);
};

} // namespace crash2
