// world_underwater.h — Crash 3's underwater scene mode (scene flag 0x8000000; crash3-decomp
// docs/notes/world-renderer.md): vertices with bit 18 sway as in the sway mode, and every corner without bit 19 is
// depth-cued by the sine sample its sway read.
#pragma once

#include "render/world_pass.h"

namespace crash3::world {

void projectUnderwaterTriangle(crash::world::Pass &pass); // 0x800428D0
void projectUnderwaterQuad(crash::world::Pass &pass);     // 0x800429E4
void drawUnderwaterTriangle(crash::world::Pass &pass);    // 0x80042994
void drawUnderwaterQuad(crash::world::Pass &pass);        // 0x80042ADC

} // namespace crash3::world
