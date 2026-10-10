---
id: 29
title: "Crash 2's new-game intro in scene 2 never ends, and Start (pause) returns to 0xA0 from FUN_80019800"
status: open
symptom: after New Game the Cortex intro loops its idle animation for over 10,000 frames; a Start press in any scene faults with "unimplemented BIOS A0:0x10"
state_items: S008, S009
tags: crash2,warp-room,intro,pause
created: 2026-10-10
updated: 2026-10-10
---

Observed: New Game requests scene 2 (the warp room); its intro keeps the Cortex close-up looping with scene id 2
and the request word at -1 through frame 12,000, so the player never gets the warp-room hub and the portal path
cannot be walked. Start taps and the other face buttons do not skip it.

Observed: a Start press (CoreLoop pause toggle at 0x800118EC, `FUN_80019800(&DAT_8006cb50,4,4,0,0,0)`) faults in
scenes 2 and 0x15 alike: `FUN_80019800` returns through a saved ra of 0xA0 (block 0x80019940), so the port
reports "unimplemented BIOS A0:0x10" with a stale t1. The pause object is created through `FUN_80019988`,
`FUN_80014260` and `FUN_8001C340`.

Next: find what the intro scripts wait on (XA end, a timer or a pad edge) and which callee overwrites the saved
ra slot of `FUN_80019800`. Then walk the hub spawn into a portal with forced input and confirm the level request.
