// object_cull.h — the screen test of Crash 1's Gfx_CalcObjectMatrices (0x800180CC, crash-bandicoot-decomp
// docs/notes/screen-culls.md) in host code, its columns widened to the presented canvas. The function's body after the
// test runs as the original. An object only the canvas keeps is drawn by its caller inside a GuestRedraw that keeps
// only its packets, off the guest clock. At 4:3 memory, GTE and registers are the retail routine's.
#pragma once

class Core;

namespace crash1 {

class ObjectCull {
public:
  static void registerOverride(Core &core);
};

} // namespace crash1
