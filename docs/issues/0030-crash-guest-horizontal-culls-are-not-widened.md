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
- Dingodile's right edge and the underwater level's side wedges still show the clear colour. It is the sort list, not
  a cull: logged per frame at 16:9 (each projected polygon rasterised into 4-pixel cells, kept or not; `warp scene
  0x03` and `0x0E`, 60 frames from a saved state), no polygon in the list covers those cells, and the only margin
  cells the renderer drops are below row 217, the retail row test (crash3-decomp `docs/notes/screen-culls.md`, "The
  world's sort list"). As in Crash 2's warp room, the list names only what the 4:3 view needs; left as is.

Crash 3 scene 0x3A (2026-11-14): the scan that warps through every scene in one process (0x02..0x2A, a shot after
each, then 0x3A) sometimes ends with exit 134, psxport's frame watchdog (no present for 3 s). The guest is not waiting
on a device. Its timeout line (`[watchdog] guest pc=…`, psxport e21c309a) and a sampler over 4000 block boundaries put
it in a `j .` at 0x800210FC: the `LDAT` init 0x80021084, run by the level load 0x80014BC8, traps when the loaded NSD's
level is not the scene id, and the scene id is 0. The request for scene 0 is GOOL's own store (op 0x1C, sub-op 0xC,
0x80036798) from object 0x800ACAB4, code word 0x1CC4DE1F at 0x800BB6B0, popping a 0 its event frame carries. It fires
exactly 541 frames after the 0x3A load in every tripped run, here and on the second machine; clean runs never make
that store within 30 s (crash3-decomp `docs/notes/scene-loads.md`). Scene 0 has no scene-table row. No natural flow
tried reaches 0x3A (boot idle, New Game to 0x02), so which event sends the 0, and whether it is state the warp skips
that the game's own route into 0x3A would set, is not traced. Not fixed.

Crash 3 scene effects and objects (2026-11-14, crash3-decomp `docs/notes/screen-culls.md`, "Seen in the picture"):
each group's right bound was raised through the debug server from one saved state per scene (0x02, 0x03, 0x04, 0x0D,
0x0E), 16:9, and the shot 60 frames later compared with the same state unpatched (two unpatched runs are identical).
- The scene effects matter: in the jet-ski level 0x0D the sea stops at jagged edges in the right and
  left margins; with the bounds raised the right margin fills (2778 pixels, nothing else changes;
  `scratch/c3left/ev/pair_0D_fx.png`). Raised one at a time, the sea is 0x80043FFC's alone (its bound, kept at
  scratchpad 0x38C, gives the same 2778 pixels; 0x800452A0's gives none here). 0x80043FFC is a hand-written grid
  drawer with its own cell routines (0x8004420C, 0x8004447C, 0x80044674, 0x80044878; about 830 instructions), the
  left edge being the SXY sign bit, so no bound moves it. Fixed since (below). 0x800452A0 (with 0x800454D4) and
  0x80043310 (scene 0x02) changed nothing in the scenes tried.
- The object box test is `D_8003DEF4` (Crash 2's 0x80041D14 in quarter units; the corner check is 0x8003E000) and
  the model drawers test each triangle against the packed bound. Raising either or both changed no margin pixel in
  the five scenes, so no override was written. Raising both lets the guest drawers draw more, which changes the
  game's own timing: later frames differ everywhere (0x04 out of phase by frame 150). A widening of the objects has
  to keep the margin's drawing off the guest clock, as the world renderer does.
- Cost (`PSXPORT_DEBUG=perf`, ires 0, fps60, 3840x2160 sink, 16:9, 15 s after the warp, two unpaced runs a build,
  run-to-run spread 0 on p50 and p99): instructions per logic frame p50 / p99, native against the guest renderer,
  scene 0x03 frame 104.9 / 134.2 M against 108.0 / 138.4 M (game-logic 86.0 against 90.2, present 17.3 against 16.8:
  the margins' polygons); scene 0x0E frame 75.5 / 112.2 M against 81.8 / 117.4-118.5 M (game-logic 64.0 against 70.3,
  present 12.2 against 11.9).

Crash 3 water grid, fixed (2026-11-14): `crash3::WaterGrid` (`titles/crash3/render/water_grid.*`) is 0x80043FFC with its
row, vertex and cell routines in host code (crash3-decomp `docs/notes/water-grid.md`). The grid walks each row from the
column the last row's leading culls left and ends the row at the first culled cell after a drawn one, so the cull
decides which cells, cache entries and GTE state the walk produces, not just which packets it links. The owner
therefore runs the retail walk (retail columns, every instruction charged to the guest clock), and at 16:9 a second
walk over the canvas's columns (`world_screen`, shared with the world renderer) that links only the cells the retail
test culls and charges nothing, after which the scratchpad and the GTE are put back as the retail walk left them.
- The grid runs in scenes 0x0D, 0x0E, 0x19, 0x1C, 0x21 and 0x27 (scene flag 0x1000000, read after each `warp scene`).
  The heading at entry selects along-near everywhere but 0x19 (across-far); along-far and across-near were not seen.
- 4:3 differential (`PSXPORT_OVERRIDE_DIFF=0x80043FFC`, every call compared): scene 0x0D from a saved state, 12 s,
  1518 of 1518 equal; every scene 0x02..0x2A and 0x3A by `warp scene` in one process, 6 s each, 3849 of 3849 equal.
- 16:9, scene 0x0D, the saved state 60 frames on (`scratch/c3water/crops_0D.png`: the unpatched frame from the
  earlier pass on top, this build under it, then left and right margins of each at 3x): the sea fills both margins;
  the left one, the sign-bit edge, had black wedges. The centre 512 columns equal the 4:3 shot pixel for pixel at
  frames 60 and 200. Main RAM after 200 frames from the state, 4:3 against 16:9: differs only in the two frames'
  ordering tables (0x61AA8-0x65B31) and primitive buffers (0x768AC-0x7BBBA, 0x8CCE4-0x92056); the scratchpad only at
  0x14, a later drawer's saved primitive cursor. Logic state is the same with the widening on and off.
- Still showing: white wedges in the top corners above the horizon in both margins (in the unpatched frame too; not
  the grid, owner not traced), and the grid's own far edge just under the horizon in the left margin.
- Cost (`PSXPORT_DEBUG=perf`, ires 0, fps60, 3840x2160 sink, `warp scene 0x0D`, 15 s, unpaced; native against the
  guest grid built without the registration; p50 spread 0 over two 16:9 runs each): instructions per logic frame p50,
  4:3 game-logic 72.4 M against 81.8 M (the grid in host code saves 9.4 M), present 10.5 against 9.0 M; 16:9
  game-logic 77.6 against 81.8 M (the margin walk costs 5.2 M of the saving), present 24.6 against 16.0 M, frame
  102.8 against 97.5 M. The present cost is the presenter's: draws and render passes per present 279 against 145 at
  16:9, and 87 against 51 already at 4:3, where the packets are byte-identical and only the native stores carry the
  vertices' unrounded places. Fixed in psxport (b5f3c79b, psxport issue 0206): a sliver's reach split the presenter's
  batch. At 4:3 both grids now make 35 draws per present (p99 58); present 8.19 against 8.06 Minsn/frame p50, from
  10.49 and 9.04.

Crash 1 (2026-11-14, crash-bandicoot-decomp `docs/notes/screen-culls.md`):
- Reproduce: from boot at 16:9 the earliest 3D screen is the intro in Cortex's castle (about frame 2100); its margins
  stop at whole-polygon edges. The warp catalogue is `warp scene` over Crash 1's scene table (no spawn warp: none was
  needed). In scene 0x06 (`warp scene 0x06`, 8 s, 60 frames from a saved state) an object in the right margin is missing
  that appears when the object cull's bounds are raised through the debug server.
- The world has no screen test. `Gte_RTransformWorlds` 0x80035438 and its five variants project and link every
  polygon the sort list names; their only compares are fog depths. The list is SLST disc data, copied or
  delta-decoded by `Slst_Update` 0x80029B0C with no GTE use. So the world's margins hold what the 4:3 list names and
  there is nothing to widen; that is why none of Crash 2/3's constants were found.
- The objects are culled before they are transformed, in `Gfx_CalcObjectMatrices` 0x800180CC (callers: the SVTX and
  CVTX transforms 0x80018964, 0x80018A40): a box test of the projected bound's centred extents against ±0x100
  columns, or a centre test |x·H/z| < 0x13D. Raising both bounds by the 86-column margin (scenes 0x03, 0x06, 0x0A,
  0x11, 0x16, 0x1A) changed only scene 0x06's right margin (216 pixels).
- Fixed at that owner: `crash1::ObjectCull` (`titles/crash1/render/object_cull.*`) is the function's head in host
  code, its column bounds over the record canvas's, resuming the original body where the retail test keeps the object.
  An object only the canvas keeps gets a reject as retail (charged as retail), then the original body and its caller's
  draw run in a psxport `GuestRedraw` that keeps only the draw context's OT, primitive tail and primitive buffer
  (dropped if the tail passes the buffer's end), so it costs the guest clock nothing and leaves no other state.
- 4:3 differential (`PSXPORT_OVERRIDE_DIFF=0x800180CC`, `scratch/c1cull/diffrun.py`, 6 s a scene): scenes 0x05-0x39
  in one process 55086 of 55086 sampled calls equal (220153 calls), 0x03 883 of 883; scenes 0x06 and 0x1D with
  every call compared 4520 of 4520. 2.5% of calls take the box test (0x08, 0x09, 0x15, 0x17-0x19, 0x1C, 0x1D, 0x1F,
  0x22, 0x23, 0x29), the rest the centre test or the 0x40000 bypass; the level flag 0x10000 that skips the test was
  never set. Scene 0x04 cannot be reached: its warp faults with or without the owner (issue 0043).
- 16:9 from saved states (`scratch/c1cull/shots/crops.png`: scene 0x06 before this change, then 60 and 200 frames on,
  then 0x18 and 0x1D): the object is drawn in the right margin of 0x06, a fruit and a platform at the right of 0x1D; the
  centre 512 columns of each 16:9 shot equal the 4:3 shot of the same frame pixel for pixel. Main RAM, 4:3 against
  16:9, differs only in the draw contexts' primitive tails, ordering tables and primitive buffers (0x06 at 60 and 200
  frames, 0x1D); 0x18 is identical.
- Cost (`PSXPORT_DEBUG=perf`, ires 0, fps60, 3840x2160 sink, 15 s after the warp, unpaced, two runs a build, p50
  spread 0): instructions per logic frame p50, native against the guest routine. 4:3 scene 0x06: equal (game-logic
  54.0 M, frame 61.3 M). 16:9 scene 0x06 game-logic 55.6 against 54.0 M, present 11.7 against 11.0 M, frame 67.1
  against 65.0 M; scene 0x1D 51.9 against 50.9 M, 11.4 against 11.0 M, 62.9 against 61.9 M. The extra is the margin
  objects' redraws and packets, run on the host off the guest clock; draws per present p99 24 against 18 (0x06) and
  32 against 28 (0x1D).
- Still empty: the world's margins wherever the sort list names no polygon (every scene), and the intro's 4:3-wide
  blurred overlay rectangle (owner not traced). Sprites, fragments and text (`Gool_ObjectTransform`'s other drawers)
  were not read for a screen test.
