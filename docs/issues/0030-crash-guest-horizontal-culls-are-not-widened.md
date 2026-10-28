---
id: 30
title: "At 16:9 the guest's own horizontal culls leave the margins partly drawn"
status: open
symptom: Crash 3 at 16:9 (Dingodile, `warp 3`) draws geometry into the margins, but floor and wall polygons stop in jagged edges and the clear colour shows beyond them
state_items: S006
tags: crash1,crash2,crash3,widescreen,culling
created: 2026-10-15
updated: 2026-10-28
---

Reproduce: `crash_port` headless with `aspect=1`, `pick crash3`, `warp 3` at frame 1200, shot at frame 2400.
The Record canvas is 684x240 and the margins hold polygons the guest submitted, so the canvas widening works;
what is missing is the guest's own rejection of primitives outside the 4:3 screen, which runs before GP0.

The edge is whole polygons, which is what both a runtime reject and the zone's precomputed polygon list
would leave. CrashEdit documents each camera's sort list as the polygons drawn on screen and their order,
indexed against the zone's world, so the world set may be fixed for the 4:3 view before any runtime test.

Crash 1 shows the same edge: `warp 3` and the Jungle Rollers attract demo at 16:9 leave the clear colour and
jagged whole-polygon edges in both margins (2026-10-28). The README carries 4:3 stills only until this closes.

Evidence so far:
- Crash 3 census (`llvm-objdump` over the whole text): no `slti`/`sltiu` against 0x1FF/0x200/0x201 sits in a
  world submitter; 0x8001D730 compares a resource size, 0x8003B82C a texture-page field.
- Crash 1 census, deleted from `titles/crash1/executable.json` in this pass (see c28dc02,
  `runtime.projection.horizontal_bound`): the GTE H global 0x800578D0 has one writer and 21 readers; the two
  that decide anything use H as the near plane (`FUN_8003A144`, band H < Z <= 11999) and a light offset
  (`FUN_8001DE78`). None is a screen-space half-width.

Next: decompile Crash 3's world submitter and trace whether a polygon leaves the camera's sort list or a
screen-space test. If it is the list, widening needs a wider list per camera, not a cull change.
