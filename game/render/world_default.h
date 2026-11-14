// world_default.h — the default scene mode's projectors and drawers, which every other mode ends in. A projector runs
// with the polygon's corners in the GTE's vectors; a drawer with its corners projected and its colours, depth and slot
// found.
#pragma once

#include "render/world_pass.h"

namespace crash::world {

void projectTriangle(Pass &pass);
void projectQuad(Pass &pass);
// The first corner's vector kept first (Crash 2 lamps and glow, Crash 3 glow).
void projectQuadKeepFirst(Pass &pass);
void drawTriangle(Pass &pass);
void drawQuad(Pass &pass);
// A black flat polygon instead: its last three corners from the GTE, a quad's first the projector wrote; the caller
// charges its site's instructions.
void drawFlatTriangle(Pass &pass);
void drawFlatQuad(Pass &pass);

} // namespace crash::world
