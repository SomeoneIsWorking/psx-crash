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

## The widescreen projection owner

`core/crash1_widescreen.*` is this title's `GuestWidescreenProjection`. Its facts are in
`executable.json` under `runtime.projection` and were read from the user-supplied bytes.

Crash 1's horizontal projection is the **GTE screen offset `OFX` (cop2 control register 24)**, not
`H` and not a viewport rectangle, and its retail 4:3 value is **zero** — the centring lives in the
title's own 3x3 view matrix. A whole-image census of all 72192 instruction words finds exactly two
control-register writers for `OFX`, two for `OFY`, two for `H`, and **zero readers of any of them**:

- `0x80042F8C` `SetGeomOffset(x, y)` — `sll $a0,16; sll $a1,16; ctc2 $a0,0xC000; ctc2 $a1,0xC800`.
  Called from `0x8001783C` (camera setup) and `0x80017F00` — **every frame**.
- `0x80042FAC` `SetGeomScreen(h)` — `ctc2 $a0,0xD000`. Called from `0x80017830` and `0x80026770`
  (every frame). Overridden only to record the value, because `H` is the *scale* and a widening holds
  it fixed.
- `0x80042B1C` `gte_init` — publishes `H = 0x3E8` (1000), `OFX = 0`, `OFY = 0` from `$zero`. One call
  site, `0x80016558` inside `Init`.

The widening is `OFX' = retail_OFX + margin`, `OFY' = retail_OFY`, `H' = retail_H`. There is no
per-frame host re-publish (the guest republishes its own centre), no guest call at a frame boundary,
and no coprocessor register written by the host. See `../../docs/issues/0015`.
