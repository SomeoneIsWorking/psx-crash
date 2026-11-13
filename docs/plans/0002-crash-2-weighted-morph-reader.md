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
scenes whose flags have bit 0 and a property 0x186 height) with a nonzero X rotation (+0x6C) and its y within 0x16A << 8 of
that height (crash2-decomp `docs/notes/frame-loop.md`). `spawn <n>` (the debug menu's spawn warp: the level's NSD spawn
points, which `func_8001F9C4` starts a load at) reaches every spawn point of the five scenes whose default is index 6
(0xD, 0x19, 0x20, 0x21, 0x23; 37 points), 12 to 15 seconds each with Crash idle and again with up held: the lit
draw step 0x80040FC4 runs thousands of times, the lit reader in none (`PSXPORT_OVERRIDE_DIFF=0x80040D14` samples 0
calls), so no object drawn there is pitched. The next place to look is a pitched two-frame object outside the default
index: an object byte +0x121 of 6 in another scene. Neither +0x121 nor +0x6C is entity data: the only native store to
the word +0x120 is `Obj_Init`'s clear (0x80019A1C) and the only read of +0x121 is the draw-step pick (0x8001AF80), so
GOOL code sets both. Finding where the lit reader can run from data therefore means reading every level's GOOL
entries for stores to those two fields and every SVTX animation's frame count; neither repository has an NSF or GOOL
reader yet. Until that scan says otherwise the lit reader's override is unproven; if it finds no such object, the
override is dead code and goes. The t = 1 probe matches the real chain in 298 of 299 frames of scene 0x15 (the other
is a declared checkpoint cut) and 299 of 299 of scene 0x6 once the replay queues in the real walk's order (issue 0038).

Guest time: each native call charges the instructions the retail reader runs on the same path (cache hit, raw, the
three delta fields with or without a word fetch and a delta, the lit GTE tail), so the guest clock is the retail
one: over 139,264 calls of scene 0x15 the retail reader, run first on the same state, advanced it by the same count in
every call, and its registers, running bytes and cache words matched the native reader's in every call.

Cost: scene 0x15 from a saved state with up held, fps60 off, `PSXPORT_DEBUG=perf`, 300 logic frames, two runs a build
on the same tree. With and without `MorphReader`, RAM, scratchpad and the guest clock match frame for frame (shown over
600 frames once psxport d4de29d4 fixed issue 0039, where the device clock charged Lightrec cycles and the override
boundary moved CNT2). The readers add about 394 host dispatches a logic frame (7,284 to 125,308). Game logic falls
from 77.209 to 76.494 Minsn a frame (-0.93%) and the frame from 88.570 to 87.846 (-0.82%), against run-to-run spreads
of 0.001 and 0.006 Minsn. Against a build whose overrides only call the retail readers, the native readers took 3.17
Minsn a frame less (game logic -3.80%; measured before d4de29d4).
