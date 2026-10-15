# Crash Bandicoot

The selected target is North American `SCUS-94900`, executable `SCUS_949.00`.
`executable.json` owns its identity, PS-X EXE header, frame, BIOS, and input facts; the verifier
compares those facts to the user-supplied bytes.

Recorded independent execution reaches the first eight calls through the
`EnterCriticalSection` wrapper `0x8003E1F8`, records Cause `0x20` and EPC `0x8003E1FC`, resumes at
`0x8003E200`, and agrees 34/34 at B(56h) with `ra=0x800431B8`. Retail disassembly of
`[0x8004319C,0x80043248)` grounds the following C0 slot-6 read, fourteen-word copy, and A(44h)
tail-dispatch with `ra=0x800431E8`. The independent chain stops at local wrapper `0x8004323C` before
that non-link tail dispatch.

The preserved compatibility route reached 1,172/1,172 fields and the 3D title menu. The BIOS auto-pad
word is `0x80057054`, published before retail `PadUpdate`. Live evidence identifies
`GfxUpdateMatrices 0x80017A14` and `GoolObjectTransform 0x8001DE78` as pre-GTE ownership candidates.

These are frozen facts, not a static-product contract: the product now maps the authenticated
executable into psxport's Lightrec executor with the same native owners. Do not emit, build, or run
generated guest code. See `../../docs/migration.md`.

## Projection

Crash 1's horizontal projection is the GTE screen offset `OFX` (cop2 control register 24), retail value
zero; the centring lives in the title's own view matrix. `SetGeomOffset` `0x80042F8C` is called every
frame from `0x80017F00`. On Record the canvas widens at 16:9 and the retail centre stays, so nothing
moves `OFX`; the issue 0015 census is the record of the measurement.
