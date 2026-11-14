---
id: 30
title: "At 16:9 the guest's own horizontal culls leave the margins partly drawn"
status: open
symptom: Crash 3 at 16:9 (Dingodile, `warp 3`) draws geometry into the margins, but floor and wall polygons stop in jagged edges and the clear colour shows beyond them
state_items: S006
tags: crash1,crash2,crash3,widescreen,culling
created: 2026-10-15
updated: 2026-11-14
---

Reproduce: `crash_port` headless with `aspect=1`, `pick crash3`, `warp 3` at frame 1200, shot at frame 2400.
The Record canvas is 684x240 and the margins hold polygons the guest submitted, so the canvas widening works;
what is missing is the guest's own rejection of primitives outside the 4:3 screen, which runs before GP0.

The edge is whole polygons. In Crash 2 it is a runtime reject, not the camera's sort list: the list carries the
margin's polygons and the projector steps drop them (below). Crash 2 shows it in every level tried:
`warp scene 0x15` at 16:9, 200 frames after the warp-in, the right margin is black past x = 640 of 684;
scenes 0x10 and 0x25 the same. The warp room and earlier screens were not checked.

Crash 1 shows the same edge: `warp 3` and the Jungle Rollers attract demo at 16:9 leave the clear colour and
jagged whole-polygon edges in both margins (2026-10-28). The README carries 4:3 stills only until this closes.

Evidence so far:
- Crash 3: no `slti`/`sltiu` against 0x1FF/0x200/0x201 sits in a world submitter, because the culls subtract a
  packed bound instead (below).
- Crash 1 census, deleted from `titles/crash1/executable.json` in this pass (see c28dc02,
  `runtime.projection.horizontal_bound`): the GTE H global 0x800578D0 has one writer and 21 readers; the two
  that decide anything use H as the near plane (`FUN_8003A144`, band H < Z <= 11999) and a light offset
  (`FUN_8001DE78`). None is a screen-space half-width.

Crash 2 owners (2026-11-14, crash2-decomp `docs/notes/screen-culls.md`):
- World polygons: the scene mode's projector steps of `Gfx_DrawWorlds` (default `func_80042420`, `func_800426A0`; the
  other modes' at 0x80045E98, 0x80046078) reject a polygon when every corner's SX is negative or every corner's SX is
  at least 512, the bound 0x00D90200 that `Gfx_DrawWorlds` writes to scratchpad 0x1F80021C. The sort list is not the
  limit: with that bound's x raised to 0x3FF through the debug server (`w32 0x80041FA4 0x371803FF`), scenes 0x15, 0x10
  and 0x25 at 16:9 fill the right margin with walls and floor and the 4:3 picture is unchanged
  (`scratch/cull/right_margin_retail_vs_bound_raised.png`: the right 200 columns, retail left, raised right). The left edge is the sign bit of the SXY words, so no
  bound moves it.
- Objects: the box test 0x80041D14 (packet word 0x1C, called `jalr $t8` by the object drawers) keeps a model when one
  of its box's eight projected corners has 0 <= SX < 0x200 and 0 <= SY < 0xD8. It returns to its entry `$ra` on every
  path, so it is a valid override key. Not reproduced: in the three scenes above no object changed with its bound
  widened, so no change was made.
- Object triangles: a routine at 0x80041BE8 applies the world test with the same bound built inline.

Proper fix for the world, and why it was not made in this pass: the projectors are reached by `jr` from
`Gfx_DrawWorlds`' loop and jump on to the draw steps and back to fixed loop addresses, so none of them is an override
key. Widening the left edge needs `Gfx_DrawWorlds` in host code with every scene mode's projector and draw step it
jumps through (0x80041E5C to about 0x80046100, some 4,000 retail instructions over the default mode and the modes
`Gfx_SetupScene` selects), testing SX against the record canvas's columns (psxport `recordCanvasColumns`). Patching
the immediate would move only the right edge and is a code patch, not an owner.

Crash 3 builds the same bound inline at ten sites (crash3-decomp `docs/notes/screen-culls.md`); the one read
(0x8003CD38) has the same all-corners-past-an-edge test with the left edge as a sign bit, so it needs the same kind of
native renderer. Crash 1 holds none of these constants; its world renderers (`RGteTransformWorlds` and its variants,
crash-bandicoot-decomp `docs/names.md`) are not yet read for a screen test.
