// water_grid.h — Crash 3's water grid (0x80043FFC, crash3-decomp docs/notes/water-grid.md) in host code: the sea's
// cells around the grid's centre, walked row by row on the half the camera faces, culled against the presented
// canvas's columns. At 4:3 its memory, GTE and packets are the retail routine's.
#pragma once

class Core;

namespace crash3 {

class WaterGrid {
public:
  static void registerOverrides(Core &core);
};

} // namespace crash3
