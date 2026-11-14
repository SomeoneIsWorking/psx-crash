// world_renderer.h — the world renderer (Crash 2 Gfx_DrawWorlds 0x80041E5C, Crash 3 0x80040ED8) in host code: the
// sort list's polygons projected, culled against the presented canvas's columns and linked into the frame's ordering
// table as the title's retail code links them. At 4:3 its memory, GTE and packets are the retail routine's.
#pragma once

#include "render/world_pass.h"

class Core;

namespace crash::world {

// One call of the title's renderer, its arguments in the guest's registers.
void drawWorlds(Core &core, const Title &title);

} // namespace crash::world
