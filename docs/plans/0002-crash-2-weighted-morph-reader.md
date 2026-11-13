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
loads were restructured. The lit reader's GTE tail was not
reached in any scene start: 0x80040FC4 takes it only for an object with a nonzero word +0x6C near the camera, so it
has no differential evidence yet. The t = 1 probe matches the real chain in 298 of 299 frames of scene 0x15 (the other
is a declared checkpoint cut) and 299 of 299 of scene 0x6 once the replay queues in the real walk's order (issue 0038).

Cost: the readers add about 1,390 host dispatches a logic frame on scene 0x15 (host_dispatches 15,984 to 570,417 over
400 frames). The instruction meter cannot isolate them: the native readers retire no guest instructions, so the two
builds' runs diverge, and the frame p50 read +10.9% (124.8 to 138.4 Minsn) from one state and -2.8% (148.9 to 144.7)
from another, each with a run-to-run spread under 0.01%.
