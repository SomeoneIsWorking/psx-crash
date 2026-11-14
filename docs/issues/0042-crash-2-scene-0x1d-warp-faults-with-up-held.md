---
id: 42
title: "Crash 2: the real frame faults at 0x80014B74 after a scene 0x1D spawn warp with up held"
status: closed
symptom: the product aborts (exit 139) right after `spawn 6` in scene 0x1D while up is held
state_items: S007
tags: crash2,crash,warp
created: 2026-11-14
updated: 2026-11-14
---
Reproduce (deterministic): `scratch/cuts/rep42.py` (boot, `warp scene 0x15`, `warp scene 0x1D`, pause, then per spawn
argument `hold FFEF`, `spawn N`, step 300 paused). From the paused state it saves in scene 0x1D
(`M_SAVE=scratch/cuts/s1d.state`), `spawn 6` with up held faults at logic frame 151 on every run, Lightrec fault at
0x80014B74, exit 139. Without the hold (`M_NOHOLD=1`) it is alive after 300 frames. `scratch/cuts/s1d_149.state` is
saved after `spawn 6` and 149 steps; load it paused (`state load`), `hold FFEF`, `step 3`, and it faults.

Traced so far:
- 0x80014B74 is the loop of `Nsf_LookupEid` (0x80014B44), which walks its NSD bucket for the EID with no bound, so an
  EID missing from the level's NSD runs it off mapped memory (the RAM mirrors to 0x807FFFFF are mapped, as on
  hardware).
- The faulting call comes through `Nsf_CloseEntry` (saved ra 0x80038F9C) from `Gool_OpChunk` (0x80038F00) op 3: a GOOL
  object asks whether entry 0x4C694073 is resident. The EID is an operand at 0x800F3628 in the EID list at 0x800F3600
  (0x4C694ED5 just before it resolves). That list belongs to the entry at 0x800F3584.
- `g_CurrentZone` is 0x800C72D0 at the frame before the fault.
- The spawn warp (`game/frame/scene_warp.cpp`) writes the spawn index 0x8006CD7C and requests the scene; the level
  start (`func_8001F9C4`) uses that index as the slot. In the C so far, retail picks a nonzero slot only for level
  kind 2 (header +8 == 2, the warp rooms), from the table at 0x8005B914 keyed by the level just left. Death sets 0
  (`800111B0.c`). Elsewhere the index carries over from the previous level start. So spawn 6 of a level that is not
  a warp room may be a slot retail never starts at.

Decided by RE (crash2-decomp `docs/notes/spawn-index.md`): retail never starts level 0x1D at spawn 6.
- The scene's level header word at 8 is 0x1D, so `func_8001F9C4` keeps the index the game left. Every store to it is
  `func_8001F9C4`, `Sys_ResetGameInfo` and the demo end (0), and GOOL `GVAW` to global 131, which stores only 0 and 1
  in every GOOL entry on the disc. The warp room's table slot is zeroed by its `CHeOC` object on its first frame (store
  watch: 6, then 0, on frame 2).
- 0x800F3584 is `WillC`'s (the player's) constant pool; its `Gool_OpChunk` kind 3 asks whether `Cda0V` (0x4C694073,
  pool word 41) is resident. Level 0x1D's NSD has no record for it, so `Nsf_LookupEid` runs off the records.

Fix: `SceneWarp::armSpawn` refuses the warp room (it picks its own spawn) and, elsewhere, spawns at or past
`SpawnPoints::carriedSpawns` (2 for Crash 2).

Proof, from `s1d.state` with up held (`rep42.py p42.log 6 1 0`): `spawn 6` replies "refused: the game starts level
0x1D only at spawn points 0-1"; `spawn 1` and `spawn 0` each run 300 frames with no fault (frames 602 and 902), and
the present after spawn 1 shows Crash on the level's ice path.
