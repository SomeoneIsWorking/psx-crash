---
id: 34
title: "Crash 3 `warp 0x19` starts the scene with a zero divisor a console would trap on"
status: open
symptom: after `warp 0x19` the guest divides by zero at 0x8001F724; a console takes the guard's break 7 into the BIOS handler
state_items: S008
tags: crash3,warp
created: 2026-10-29
updated: 2026-10-29
---

Reproduce: `crash_port` headless, `pick crash3`, `warp 0x19`. A few frames into the scene load the block at 0x8001F724
(ra 0x8001F398) divides v1 = 0x2D6FFC by a1 = 0. The compiler's `bnez; break 7` guard follows the divide, so on a
console this path enters the BIOS break handler; in the port Lightrec strips the guard (issue 0033) and the scene
plays on with the MIPS result (LO = -1, HI = dividend).

Owner: the title's scene warp. Open: which guest word feeds a1 at 0x8001F724, and which step of the game's own route
to scene 0x19 sets it before the scene starts. Until then the jet-ski scene reached by the warp is not a state the
console reaches, and no console oracle has compared it.
