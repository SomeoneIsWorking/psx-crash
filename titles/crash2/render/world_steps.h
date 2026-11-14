// world_steps.h — Crash 2's scene modes' projectors and drawers of Gfx_DrawWorlds in host code (crash2-decomp
// docs/notes/world-renderer.md). A projector runs with the polygon's corners in the GTE's vectors; a drawer with its
// corners projected and its colours, depth and slot found. Each mode composes the default steps as the retail one jumps
// into them.
#pragma once

#include "render/world_default.h"
#include "render/world_pass.h"

namespace crash2::world {

// Crash 2's modes are written over the shared pass and default steps.
using namespace crash::world;

// Fog (flag 0x200000 with D_8006CDC4): each corner toward a bright or dark far colour by the scene's depth.
void drawFogTriangle(Pass &pass); // 0x800459A8
void drawFogQuad(Pass &pass);     // 0x80045AB4
// Depth fog (flag 0x20, or 0x200000 without): corners past a depth toward the far colour; all black, flat.
void drawDepthFogTriangle(Pass &pass); // 0x800439D4
void drawDepthFogQuad(Pass &pass);     // 0x80043A84
// Tint (flag 0x8000): bit 19's corners toward the tint's colour.
void drawTintTriangle(Pass &pass); // 0x800449B0
void drawTintQuad(Pass &pass);     // 0x80044A04
// Pulse (flag 0x40): bit 19's corners pulse with the clock; else, the first vertex's bit 18 set, corners dim by depth.
void drawPulseTriangle(Pass &pass); // 0x80043D84
void drawPulseQuad(Pass &pass);     // 0x80043EA0
// Lamps (flag 0x100): corners lit from two objects' positions.
void drawLampTriangle(Pass &pass); // 0x80042E84
void drawLampQuad(Pass &pass);     // 0x80042F08
// Glow (flag 4 or 0x800000): corners dimmed by their distance from two objects; far from both, black and flat.
void drawGlowTriangle(Pass &pass); // 0x80044E48
void drawGlowQuad(Pass &pass);     // 0x80044EC0
// Waves (flag 0x200): bit 18's vertices moved along y; polygons all of bit 19 dim by depth.
void projectWaveTriangle(Pass &pass); // 0x80043B74
void projectWaveQuad(Pass &pass);     // 0x80043C48
void drawWaveTriangle(Pass &pass);    // 0x80043C2C
void drawWaveQuad(Pass &pass);        // 0x80043D34
// Sway (flag 8): bit 18's vertices moved by a sine; bit 19's corners recoloured.
void projectSwayTriangle(Pass &pass); // 0x80042AB8
void projectSwayQuad(Pass &pass);     // 0x80042B7C
void drawSwayTriangle(Pass &pass);    // 0x80042B44
void drawSwayQuad(Pass &pass);       // 0x80042C2C
// The light around Crash (game flag 0x1000000): polygons near him divided into a lit grid (world_light.cpp).
void projectLightTriangle(Pass &pass); // 0x80045D88
void projectLightQuad(Pass &pass);     // 0x80045F40

} // namespace crash2::world
