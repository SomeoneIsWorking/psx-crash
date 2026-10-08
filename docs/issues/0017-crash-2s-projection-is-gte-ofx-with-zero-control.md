---
id: 17
title: "Crash 2's projection is GTE OFX with zero control-register readers, so the widening is Crash 1's — but its H is the near plane AND a HUD rectangle scalar"
status: open
symptom: The widescreen owner exists and is gated, but Crash 2 still refuses to boot, so no wide frame is ever presented
state_items: S006
tags: crash2,projection,widescreen,re-census,near-plane
created: 2026-09-27
updated: 2026-09-27
---

Every claim below is read from the authenticated `SCUS_941.54` (327,680 bytes, sha256 `6e5b2449…`,
matching `titles/crash2/executable.json`).

## What Crash 2's projection is, with the instruction words

A whole-image census of all **81,408** instruction words finds 1,024 opcode-0x12 (COP2) words — 320
`mfc2` data reads, 17 `cfc2` control reads, 332 `mtc2` data writes, 198 `ctc2` control writes, and 157
GTE commands. Exactly **two** control writers each for the three projection registers:

| register | writers | readers |
|---|---|---|
| CR[24] OFX | 0x8004EC9C, 0x8004EFF0 | **0** |
| CR[25] OFY | 0x8004ECA0, 0x8004EFF4 | **0** |
| CR[26] H | 0x8004EC7C, 0x8004F008 | 2 — 0x8004457C, 0x800447C4 |

The three publication entries, and the words that fix their semantics:

```
0x8004EC78  240803E8  addiu $t0,$zero,1000     }  projection init 0x8004EC30, called once from
0x8004EC7C  48C8D000  ctc2   $t0,0xD000       }  0x80015640 (word 0x0C013B0C). Also publishes
0x8004EC9C  48C0C000  ctc2   $zero,0xC000     }  ZSF3=0x155, ZSF4=0x100, DQA=0xEF9E, DQB=0x01400000
0x8004ECA0  48C0C800  ctc2   $zero,0xC800     }

0x8004EFE8  00042400  sll    $a0,$a0,16       }  set_geom_offset 0x8004EFE8, two call sites:
0x8004EFF0  48C4C000  ctc2   $a0,0xC000       }  0x800179CC and 0x80017F70, both word 0x0C013BFA
0x8004EFF4  48C5C800  ctc2   $a1,0xC800       }

0x8004F008  48C4D000  ctc2   $a0,0xD000       }  set_geom_screen 0x8004F008, four call sites:
                                                 0x80016EC4, 0x800179C0, 0x80020890, 0x8002F8B4,
                                                 all word 0x0C013C02
```

So the retail 4:3 projection is **OFX = 0, OFY = 0, and a per-camera-mode H**, with H = 1000 at boot
from `gte_init` and 288 from the camera setup `FUN_8001798c` at 0x800179C0. A resident-word scan finds
**zero** pointer-table entries equal to any of the three entries, so every call site is a `jal` and
`$r31 - 4` identifies it exactly — which is what lets the owner refuse an unmeasured call site instead
of guessing one.

## The hazard: H is the near plane, and H is also a HUD scalar

H is kept in the main-RAM global **`0x80060884`**, loader-created and outside the executable's own text
(`SCUS_941.54` is `0x800` header + `0x4F800` text and nothing else). Eleven sites use it — four
writers, seven readers — recorded in `titles/crash2/executable.json`. Nine are `lui 0x8006` plus a
16-bit displacement; the other two, `0x800179C4` and `0x80020310`, are pointer-form writes, which is
why the list is the authority and a displacement scan alone is not.

Two of the seven readers turn H into a **gameplay-visible** decision:

- **`FUN_8003d3cc` [0x8003D3CC,0x8003DA18)** rejects a vertex when `NOT (H < Z < 12000)`, where Z is
  the depth its own `rtps` produced. The far limit is the word `0x24012EE0` at 0x8003D4D4. **H is the
  GTE near plane**, exactly as in Crash 1, and the bound arrives as its 6th argument from
  `FUN_8001bd84` at 0x8001BDD0.
- **`FUN_8001645c` [0x8001645C,0x80016A68)** computes
  `(H * fog * 0xAA >> 20) - 0x6C` and uses it as a 2D overlay rectangle's Y and height, clamped to
  0..216 (`if (unaff_s2 < 0 || 0xd6 < unaff_s2)`). So H is a **HUD scalar**, not only a depth bound.

The other five are render-only: two `H/2` GTE light-intensity terms (`FUN_8001856c` at 0x80018614,
`FUN_800186d8` at 0x80018900), the writer's own change-detection compare at 0x80016EB8, a plain re-send
in `FUN_8002f6c8`, and two copies into the MVMVA translation vector's Z (CR[7]) at 0x8004457C and
0x800447C4 — the second reached as entry **0x800447DC** of the GTE command table at 0x8005B698, which
no call-target scan can see. Neither `cfc2` reaches a branch: both feed `ctc2 $t7,$7` immediately.

## The literal cull scan and its null

Over all 81,408 words, 13 immediate matches against a 4:3 dot width or a PSX visibility/pad mask, of
which the 4:3-width ones are `ori $4,0x0120` / `ori $2,0x0120` / `ori $5,0x0160` / `ori $2,0x0180` /
`ori $3,0x0180` / `ori $2,0x0180` / `addiu $2,$29,368` / `addiu $3,$3,-384` — bitmasks and stack
arithmetic, not compares. **NULL, stated: a cull against a variable bound carries no immediate and is
invisible to this scan.** It does not claim this image has no variable-bound cull; the census above is
what looks for those, and it found no screen-space cull among the eleven.

The draw area is the PSX default whole-display area — `lui $v1,0xE100` at 0x8004EB44 and
`sw $v0,0($a1)` at 0x8004EB64 store `(GPUSTAT & 0x3FFF) | 0xE1001000` — so this title has no second,
narrower clip rectangle for a widening to move.

## What was built

`titles/crash2/render/widescreen/crash2_widescreen.*` over the shared rule in
`game/render/widescreen/guest_projection_publication.*`. `Crash2Runtime::guestWidescreenProjection()` now returns the
owner instead of `nullptr`.

The widening moves **OFX** and holds **OFY** and **H**. It is safe here for one measured reason and not
for the one Crash 1 had to argue: **zero `cfc2` control reads of CR[24] and CR[25] anywhere in the
image**, and both call sites compute their centre from a guest global or a constant
(`FUN_80017bc4` at 0x80017F70 passes `_DAT_8006CC20 + 0x100`, and `DAT_8006CC20` is live). So the value
arriving at the leaf can never already carry the margin, and `retail + margin` is idempotent by
construction rather than by a guard.

`tests/crash2_widescreen.cpp` pins 4:3 identity exactly at a non-zero retail centre, the 16:9 margin,
the ride on a live centre, unwidening, both measured call sites widening, H untouched, the guest RAM
bound, and an install proof with no HLE plan in existence.

## What this does not establish

No live leg. `BoundaryRuntime::registerOverrides` is `final` and Crash 2 still refuses to boot, so the
three measured overrides are **installed by the test and not by the product**, and the owner's runtime
hookup is the second half of a seam the boot work has to open. Nothing here shows a wide frame on
screen. The `H/2` light terms and the two translation-vector copies were read from the decompilation
and their instruction words verified; the other four readers of the bound were **not** decompiled, and
the eleven-site census is the honest denominator for what is known about them.
