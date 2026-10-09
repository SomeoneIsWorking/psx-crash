---
id: 27
title: "Crash 2 reaches the warp-room hub but no gameplay scene; scene ids 4 and 0x14 fault in the guest allocator"
status: open
symptom: warp with any valid id 0xC to 0x28 loads a warp-room variant; id 4 or 0x14 faults in the guest heap allocator; id 0x2A hangs until the frame watchdog
state_items: S008, S009
tags: crash2,warp,scene-table,allocator
created: 2026-10-09
updated: 2026-10-09
---

Observed: after a request for scene 4 or 0x14 (word 0x8005F3F0) the heap roving pointer DAT_8006074C turns
into garbage within about 22,800 cycles of the first frame; the chain from DAT_80062BAC is intact before
the request. Expected: a level load through FUN_80014C68 and the scene table at 0x80069034 (12 bytes per
id, rows for ids 2 to 0x1A only).

Next: decompile FUN_8001148C and FUN_800116D4 around the failing request, find which row field the
level loader (FUN_800123F0) feeds the allocator, and check whether ids need the warp-room save state
that normal play sets first. Reproduce with `warp 0x14` after the hub is running.
