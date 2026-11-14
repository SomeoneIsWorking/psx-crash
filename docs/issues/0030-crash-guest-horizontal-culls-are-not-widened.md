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
scenes 0x10 and 0x25 the same. (Fixed for the world since: below.)

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

Crash 2 world, fixed (2026-11-14): `crash2::WorldRenderer` (`titles/crash2/render/world_*`) is `Gfx_DrawWorlds` in
host code with all ten scene modes (crash2-decomp `docs/notes/world-renderer.md`). Its column test is against the
record canvas's columns (`recordCanvasColumns`); the row test stays the retail packed one. A polygon only the canvas
keeps is charged to the guest clock as the retail cull left it, so the margins cost the game no time.
- 4:3 differential (`PSXPORT_OVERRIDE_DIFF=0x80041E5C`, every reachable scene by `warp scene`, 6-8 s each): 10284
  sampled calls of 40946, all equal (memory, scratchpad, GTE, callee-saved registers); default, depth fog, tint,
  pulse, lamps, glow, waves and sway were reached.
- Fog (crash2-decomp `docs/notes/world-renderer.md`, "Where fog is selected"): only level 0xF's zones carry the flag,
  and `LighC`'s lightning sets the global it also needs. `warp scene 0xF` then `spawn 1` (a spawn the game uses) reaches
  it with no input. 4:3 differential there, every call compared (`PSXPORT_OVERRIDE_DIFF_EVERY=1`, 40 s): 5422 calls,
  5422 equal; a store watch on the scene's setup word puts 895 of those frames in fog, the rest depth fog.
- Light ("Where light is selected"): the player's dark-level code `WilnC` sets `g_FrameFlags` 0x1000000 only while the
  player stands on floor type 15 of levels 0x15 and 0x24; no spawn the game uses stands there, so no warp reaches it
  and it was not added. With the flag's GOOL word and global 144 set through the debug server (`w32`), 4:3, every call
  compared, 30 s: scene 0x15 4476 of 4476 equal, scene 0x24 4462 of 4462, light's setup in all but 3 frames each. No
  polygon there passes the light projectors' test, so the grid walk (0x800461E0) is still not compared.
- 16:9: scenes 0x15, 0x10, 0x25 and the warp room (0x02) fill both margins with the world, the 4:3 picture
  unchanged; main RAM after 200 frames from one state differs between 4:3 and 16:9 only in the two frames' ordering
  tables and primitive buffers.
- The warp room's right margin past the Snow Go doorway still shows the clear colour. It is the sort list, not a cull:
  logged per frame at 16:9 (`warp scene 0x02`), the list's polygons reach SX -376 to 664 against the canvas's right
  column 598, but none covers SX 560-595 at SY 30 or 580-595 at SY 90. The list is SLST disc data for the 4:3 view,
  copied or delta-applied by `func_80029800` with no screen test (crash2-decomp `docs/notes/screen-culls.md`), so there
  is no visibility owner to override; filling the gap needs polygons the list never names. Left as is.
- Cost (`PSXPORT_DEBUG=perf`, ires 0, fps60, 3840x2160 sink, 15 s after the warp, two unpaced runs a build, run-to-run
  spread 0 on every percentile): instructions per logic frame p50 / p99, native against the guest renderer, scene 0x15
  frame 100.7 / 163.6 M against 116.4 / 180.4 M (game-logic 64.5 against 72.4, replay 18.4 against 25.7, present 18.1
  both); scene 0x02 frame 120.6 / 176.2 M against 146.8 / 197.1 M (game-logic 74.4 against 86.0, replay 30.7 against
  43.5). Paced, hidden window, on the idle second machine, `warp scene 0x15` 20 s: native presents p50 16.75 ms, p99
  18.25 ms, cadence 0.50 tick on 1189 of 1200 vblanks (0.00 x3, 1.00 x1, 2.00 x1, 5 unknown, load holds included);
  the guest renderer p99 33.75 ms, 1195 of 1208 (0.00 x6). The native renderer costs no presents.
- Objects: no object was seen to pop at the margins in these scenes; the box test 0x80041D14 is unchanged.

Crash 3 builds the same bound inline at ten sites (crash3-decomp `docs/notes/screen-culls.md`): one in the world
renderer `Gfx_DrawWorlds` 0x80040ED8, three in scene effects drawn after the world, five in object drawers, one whose
caller is not traced. All test every corner past one edge, the left edge being the SXY sign bit.

Crash 3 world, fixed (2026-11-14): its `Gfx_DrawWorlds` is Crash 2's with title numbers (quarter-unit vertices, AVSZ
depth, the scene words one slot later, three link tails; crash3-decomp `docs/notes/world-renderer.md`), so
`crash::world` (`game/render/world_*`) now serves both titles over each title's `world::Title`; sway, pulse and glow are
shared too (`world_modes`), and `crash3::WorldRenderer` adds the underwater mode.
- Reproduce: every scene by `warp scene` at 16:9 showed jagged whole-polygon margins; Dingodile (0x03) and the
  underwater level (0x0E) clearly (`scratch/c3cull/repro/`). The earliest-from-boot screen was not taken.
- 4:3 differential (`PSXPORT_OVERRIDE_DIFF=0x80040ED8`, 6 s a scene): default (scenes 0x02-0x05) 653 of 653 sampled
  calls equal, sway (0x0B, 0x0C, 0x10) 662 of 662, glow (0x24, 0x25) 533 of 533. Underwater (0x0E, 0x1C): 536 of 537
  differed, with no register different in the logged ones. Every difference matched against a per-polygon trace of the
  host drawer (18 ranges, scene 0x0E) is the last corner's colour word (GT4 + 0x28, GT3 + 0x1C) of a polygon whose last
  corner is depth-cued: the drawers (0x800429DC, 0x80042B3C) read that DPCS result with an `mfc2` in the delay slot of the
  branch back to the default drawer, and under Lightrec the guest stores a stale value there instead, seen in the
  guest-rendered picture as bright green and blue corners (`scratch/c3cull/repro/scene0E_1.png`). Cause: with
  psxport's `cop2_notify`, Lightrec's MFC/CFC C wrapper `lightrec.c:lightrec_mfc_cb` wrote `rt` directly, and the
  branch's load-delay check then wrote `rt` from `temp_reg`, which that path never loaded. Fixed in the Lightrec fork
  (5aa9bdf, psxport issue 0205): the guest drawer now stores the DPCS result, and the differential is 603 of 603
  equal at 0x0E and 646 of 646 at 0x1C, every call compared.
- Pulse and the own-slot link tail are selected at no level entry and were not compared.
- 16:9 (`scratch/c3cull/shots/pairs.png`: 4:3 left, 16:9 right, the same frame 200 frames after one saved state;
  Dingodile 0x03, underwater 0x0E, sway 0x0C, glow 0x24): the margins fill with the world and the centre 512 columns of
  each 16:9 shot equal the 4:3 shot pixel for pixel. Main RAM differs only in the two frames' ordering tables and
  cursors (0x61A88-0x65BC8) and primitive buffers, the scratchpad only in the renderer's dead stack.
- Left: Dingodile's right edge and the underwater level's top corners still show the clear colour in places; not yet
  traced to the sort list or a cull. The scene effects and object drawers still cull at x = 512.
- Cost (`PSXPORT_DEBUG=perf`, ires 0, fps60, 3840x2160 sink, 16:9, 15 s after the warp, two unpaced runs a build,
  run-to-run spread 0 on p50 and p99): instructions per logic frame p50 / p99, native against the guest renderer,
  scene 0x03 frame 104.9 / 134.2 M against 108.0 / 138.4 M (game-logic 86.0 against 90.2, present 17.3 against 16.8:
  the margins' polygons); scene 0x0E frame 75.5 / 112.2 M against 81.8 / 117.4-118.5 M (game-logic 64.0 against 70.3,
  present 12.2 against 11.9).

Crash 1 holds none of these constants; its world renderers (`RGteTransformWorlds` and its variants,
crash-bandicoot-decomp `docs/names.md`) are not yet read for a screen test.
