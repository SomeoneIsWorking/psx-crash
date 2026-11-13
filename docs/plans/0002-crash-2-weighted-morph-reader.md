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

1. A native override of `func_8003ED14`, written from the assembly (the
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
loads were restructured. Known gap: the lit reader `func_80040D14` stays guest code, so a lit, tilted
two-frame object would draw its quarter poses at the equal weight and step at the tick rate. It runs only for a
two-frame object of draw index 6 (byte +0x121, or the scene default: 6 when the scene flags have bit 0 and a property
0x186 height) with a nonzero X rotation (+0x6C) and its y within 0x16A << 8 of that height (crash2-decomp
`docs/notes/frame-loop.md`). `spawn <n>` reached every spawn point of the five index-6 scenes (0xD, 0x19, 0x20, 0x21,
0x23; 37 points), Crash idle and with up held: the lit draw step 0x80040FC4 ran thousands of times, the lit reader in
none (`PSXPORT_OVERRIDE_DIFF=0x80040D14` sampled 0 calls). GOOL code sets both +0x121 and +0x6C (the only native store
to the word +0x120 is `Obj_Init`'s clear at 0x80019A1C), so the native override shipped unproven was removed rather
than kept. If a player sees such an object step: reach it with `spawn`, confirm the lit reader runs there
(`PSXPORT_OVERRIDE_DIFF=0x80040D14` reports calls), then write its override after the unlit one (its GTE tail past
the sum, 0x80040F58..0x80040F98, 15 instructions) and prove it with the same differential. The t = 1 probe matches the real chain in 298 of 299 frames of scene 0x15 (the other
is a declared checkpoint cut) and 299 of 299 of scene 0x6 once the replay queues in the real walk's order (issue 0038).

Four frames (2026-11-14): a tick pair whose four frames are three or four distinct frames (scene 0x6's rings at
four indices a tick, (a, b) to (c, d); Crash's run at (a, b) to (b, c); animation switches) is drawn as their mean.
The packet keeps the pair that differs, since `Gfx_QueueObject` sends a pair of one frame to the static drawer, and
the other pair is read beside it in host state. Its two streams open at `func_80043658` (overridden: the original
runs, then they start at each frame's data offset) and follow the packet's fetches and deltas under the same
model's control stream (crash2-decomp `docs/notes/frame-loop.md`, Animation). A temporary probe read every replayed
packet's own pair as the second pair and matched the guest's running bytes in 369,007 of 369,007 vertex reads
(scenes 0x6 and 0x15, delta and raw paths). Scene 0x6 respawn, `scratch/fade/crash4_AB_675_676.png` and
`rings4_AB.png` (top: the later pair, as before; bottom: the mean): at the in-between Crash's arms are between real
675's raised arms and real 676's level ones, where before they were already at 676's. Real presents are identical
between the two builds (76 of 76).

Guest time: each native call charges the instructions the retail reader runs on the same path (cache hit, raw, the
three delta fields with or without a word fetch and a delta), so the guest clock is the retail
one: over 139,264 calls of scene 0x15 the retail reader, run first on the same state, advanced it by the same count in
every call, and its registers, running bytes and cache words matched the native reader's in every call.

Cost: scene 0x15 from a saved state with up held, fps60 off, `PSXPORT_DEBUG=perf`, 300 logic frames, two runs a build
on the same tree. With and without `MorphReader`, RAM, scratchpad and the guest clock match frame for frame (shown over
600 frames once psxport d4de29d4 fixed issue 0039, where the device clock charged Lightrec cycles and the override
boundary moved CNT2). The readers add about 394 host dispatches a logic frame (7,284 to 125,308). Game logic falls
from 77.209 to 76.494 Minsn a frame (-0.93%) and the frame from 88.570 to 87.846 (-0.82%), against run-to-run spreads
of 0.001 and 0.006 Minsn. Against a build whose overrides only call the retail readers, the native readers took 3.17
Minsn a frame less (game logic -3.80%; measured before d4de29d4).
