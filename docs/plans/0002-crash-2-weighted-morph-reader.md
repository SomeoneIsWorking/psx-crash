---
id: 2
title: "Crash 2: a weighted morph vertex reader for half-rate animations"
status: resolved
issues: 37
state_items: S007
created: 2026-11-13
---

Half-rate animations (Crash's run cycle, the warp-in rings) alternate a held pair (a, a) with a moving one (a, b). The
halfway pose between ticks is a quarter weight, which the retail reader cannot draw, so these poses step at 30 Hz.
The interface and the exact weighted sum are in crash2-decomp `docs/notes/frame-loop.md` (Animation).

1. Native overrides of `func_8003ED14` and its lit counterpart `func_80040D14`, written from the assembly (the
   decomp's equivalent C is GPL-3.0: evidence only). Every register the routine writes ($v0, $a0-$a3, $at, $t3,
   $t4, $t8, $t9, $s4, $s5, $gp, $sp) ends as the guest's does, and the cache words and the 0x110-0x116 deltas are
   stored alike. With k = 2 the output is the retail sum, so real draws and the t = 1 replay stay byte-identical.
2. The weight comes from the packet. `crash2::pose_blend` picks the pair and k at `Gfx_QueueObject` (multisets of
   the two ticks' four frames: x x y y gives k = 2, x x x y gives (x, y) with k = 1, x y y y gives k = 3) and keys
   it by the queued packet. The morph draw steps that call the readers (0x8003EF88, `func_8003F1D8`,
   `func_8003F410`, `func_800417B0` and the lit one) are entered with the packet in $sp, so overrides there set the
   reader's weight for the packet's draw and clear it on return.
3. Proof: Crash's run in scene 0x15 held up and the rings in scene 0x6, `fps60dump` crops in consecutive presents,
   each in-between halfway between its neighbours; a t = 1 replay byte-identical to the real chain; `[perf] cpu`
   instructions per logic frame against the run-to-run spread, since every morph vertex then goes through host code.

Shipped (2026-11-13): `titles/crash2/render/morph_reader.*` and `pose_blend`'s quarter weights (issue 0037). The weight
is set only inside a replay: the real draw runs after the replays over the same packet addresses. The box at 0x118 and
0x11C (`func_800436F4`) is weighted with the bytes. The unlit reader matches the original in 61,780 of 61,780 sampled
calls (`PSXPORT_OVERRIDE_DIFF`, scenes 0x15 and 0x6 with fps60 off), and in scenes 0xD and 0x19 before its unaligned
loads were restructured. The lit reader has no differential evidence yet: it runs only for a two-frame object of draw index 6 (the default in
scenes whose flags have bit 0 and a property 0x186 height) with a nonzero word +0x6C and its y within 0x16A << 8 of that
height (crash2-decomp `docs/notes/frame-loop.md`), and the level starts of the five such warps (scenes 0xD, 0x19, 0x20,
0x21, 0x23) hold no such object. Reaching one needs a title-owned warp past a level's start (a zone or checkpoint warp
through `Sys_LoadCheckpoint`) and a scan of each zone's objects for the three conditions. The t = 1 probe matches the real chain in 298 of 299 frames of scene 0x15 (the other
is a declared checkpoint cut) and 299 of 299 of scene 0x6 once the replay queues in the real walk's order (issue 0038).

Guest time: each native call charges the instructions the retail reader runs on the same path (cache hit, raw, the
three delta fields with or without a word fetch and a delta, the lit GTE tail), so the guest clock is the retail
one: over 139,264 calls of scene 0x15 the retail reader, run first on the same state, advanced it by the same count in
every call, and its registers, running bytes and cache words matched the native reader's in every call.

Cost: `s15_run` with fps60 off, `PSXPORT_DEBUG=perf,perfframe`, two runs a build. The readers add about 470 host
dispatches a logic frame (8,474 to 148,722 over 300). With and without `MorphReader` the guest clock and Crash's
position match frame for frame through frame 63; there game-logic falls from 77.230 to 76.567 Minsn a frame (-0.86%) and
the frame from 89.925 to 89.27 (-0.73%), against a run-to-run spread of 0.006 Minsn. The runs parted from frame 64
until psxport d4de29d4 (issue 0039: the device clock charged Lightrec cycles, so the override boundary moved CNT2);
since then RAM, scratchpad and the guest clock match frame for frame over 600 frames. Against a build whose overrides
only call the retail readers, over all 300 frames, the native readers take 3.17 Minsn a frame less (game-logic -3.80%).
