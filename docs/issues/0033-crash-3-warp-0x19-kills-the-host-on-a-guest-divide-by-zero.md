---
id: 33
title: "Crash 3 `warp 0x19` kills the host with SIGFPE on a guest divide by zero"
status: open
symptom: the product exits without a message a few frames after `warp 0x19`
state_items: S008
tags: crash3,warp,lightrec
created: 2026-10-28
updated: 2026-10-28
---

Reproduce: `crash_port` headless, `pick crash3`, `warp 0x19`; the process dies with SIGFPE (core dump) while the scene
loads. Scenes 0x10-0x14, 0x16, 0x18, 3 and 5 load.

At the fault the host executes Lightrec's `div %r14` with r14 = 0: guest `divu` of v1 = 0x2D6FFC by a1 = 0 in the
block at 0x8001F724 (ra 0x8001F398). Lightrec's `lightrec_remove_div_by_zero_check_sequence` (optimizer.c) drops the
compiler's `bnez; break 7` guard after the divide and flags it `LIGHTREC_NO_DIV_CHECK`, so a zero divisor reaches the
x86 divider instead of the guest's BREAK.

Two owners, both open: the shared Lightrec fork must keep a host-safe divide when it removes the guest guard (MIPS
divide by zero never traps), and the scene warp must find why scene 0x19 starts with a zero divisor (a precondition
the game's own route to that scene sets).
