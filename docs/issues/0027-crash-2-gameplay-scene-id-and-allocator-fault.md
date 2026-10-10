---
id: 27
title: "Crash 2 warp ids 4, 0x14 and 0x2A fault because the scene table has no row for them; the other ids already load levels"
status: resolved
symptom: warp with id 4 or 0x14 faulted in the guest heap allocator and id 0x2A hung until the frame watchdog; ids 0xC to 0x28 looked like warp rooms
state_items: S008, S009
tags: crash2,warp,scene-table,allocator
created: 2026-10-09
updated: 2026-10-10
---

Cause: `SceneWarp::arm` (`game/frame/scene_warp.cpp`) accepted every id up to 0x3C, but the scene table at
0x80069034 (12 bytes per id: header sector, header byte size, end sector) has empty rows for ids 0, 1, 4, 5, 0xB,
0x14 and 0x2A to 0x3A. `FUN_800123F0` sizes its block from the row's byte size and its sector count from the same field, so a zero row
allocates a zero-byte block through `FUN_8001148C` and reads zero sectors into it; the guest never requests those
ids. The live symptom is a watchdog hang (observed for 4, 0x14 and 0x2A); the exact loop inside the loader was not
traced. Fixed at the owner: `arm` refuses an id whose row masks to
size 0, using `FrameProgram::sceneTable`; `tests/crash_scene_warp.cpp` covers it.

The earlier reading that ids 0xC to 0x28 load "warp-room variants" was wrong. Every populated id from 0xA to
0x27 starts Crash on a pad in front of a round door and runs out into a level; `warp 0x15`, `0x10`, `0x25` and
`0x1B` each drove Crash running and (0x15) jumping in a distinct level under forced input. Ids 6 to 9 are bosses,
2 the new-game intro scene (the warp room), 3 and 0x28 and 0x29 cutscenes, 0x3B the continue screen, 0x3C the
logo and title.

Request path: a script op (`FUN_80037698`, class 0xC sub 9, 0x80037DC8) stores `operand >> 8` to the request word
0x8005F3F0; `FUN_80011800` (CoreLoop) consumes it through `FUN_8001521C` (unload) and `FUN_80014C68` (load),
which stores the id to 0x8005F3EC. The warp writes the same word in the same id space.

Open: the warp-room portal itself is not walked yet; see issue 0029.
