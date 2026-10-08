---
id: 18
title: "Crash 3 reads its own published centre back out of CR[24], so a retail+margin widening is not idempotent there and the owner needs a per-call-site policy"
status: open
symptom: The owner and its per-call-site policy are built and gated, but Crash 3 refuses to boot, so the frame-ordering consequence of the read-back is unmeasured
state_items: S006
tags: crash3,projection,widescreen,re-census,near-plane,read-back,idempotence
created: 2026-09-27
updated: 2026-09-27
---

## What Crash 3's projection is, with the instruction words

A whole-image census of all **82,944** instruction words finds 932 opcode-0x12 (COP2) words — 294
`mfc2` data reads, 34 `cfc2` control reads, 290 `mtc2` data writes, 177 `ctc2` control writes, 229 GTE
commands. Exactly **two** control writers each, and — unlike Crash 1 and Crash 2 — **one reader each**:

| register | writers | readers |
|---|---|---|
| CR[24] OFX | 0x8004F3E8, 0x8004F70C | **1** — 0x8004F6E4 |
| CR[25] OFY | 0x8004F3EC, 0x8004F710 | **1** — 0x8004F6E8 |
| CR[26] H | 0x8004F3C8, 0x8004F724 | 1 — 0x8003E6A0 |

```
0x8004F3C4  240803E8  addiu $t0,$zero,1000     }  projection init 0x8004F37C, called once from
0x8004F3C8  48C8D000  ctc2   $t0,0xD000       }  0x800154D8 (word 0x0C013CDF). Also publishes
0x8004F3E8  48C0C000  ctc2   $zero,0xC000     }  ZSF3=0x155, ZSF4=0x100, DQA=0xEF9E, DQB=0x01400000
0x8004F3EC  48C0C800  ctc2   $zero,0xC800     }

0x8004F704  00042400  sll    $a0,$a0,16       }  set_geom_offset 0x8004F704, four call sites:
0x8004F70C  48C4C000  ctc2   $a0,0xC000       }  0x8001892C, 0x80018C04, 0x8001CFC4, 0x8001D09C,
0x8004F710  48C5C800  ctc2   $a1,0xC800       }  all word 0C013DC1
0x8004F724  48C4D000  ctc2   $a0,0xD000       }  set_geom_screen 0x8004F724, six call sites
```

**The two leaves are byte-identical to Crash 2's.** `set_geom_offset`'s body sha256 is `9aa95b09…` in
both images and `set_geom_screen`'s is `d8c79a8c…` in both. That is direct evidence about the binaries
and is why the rule lives in `game/render/widescreen/guest_projection_publication.*` rather than in either title.

A resident-word scan finds **zero** pointer-table entries equal to any of the three entries, so every
call site is a `jal` and `$r31 - 4` identifies it.

## THE FINDING THAT MAKES THIS TITLE DIFFERENT: it reads the centre back

```
0x8004F6E4  4848C000  cfc2 $t0,$24     CR[24] OFX   }  FUN_8004f6e4 [0x8004F6E4,0x8004F704),
0x8004F6E8  4849C800  cfc2 $t1,$25     CR[25] OFY   }  called ONCE, from 0x8001CF8C
0x8004F6EC  00084403  sra  $t0,$t0,16                }
0x8004F6F0  00094C03  sra  $t1,$t1,16                }
0x8004F6F4  AC880000  sw   $t0,0($a0)                }
0x8004F6F8  ACA90000  sw   $t1,0($a1)                }
```

and `FUN_8001cd80` [0x8001CD80,0x8001D170) hands the result straight back:

```
0x8001CF8C  jal 0x8004F6E4      read the published centre
...
0x8001D094  lw $a0,184($sp)     }  the two stack slots FUN_8004f6e4 wrote
0x8001D098  lw $a1,188($sp)     }
0x8001D09C  jal 0x8004F704      SetGeomOffset(that centre)
```

**Consequence, and it is the whole reason this issue exists.** The Crash 1 widening's idempotence rests
on the base being `$a0` "as it stands right now", which is sound only while the value arriving at the
leaf does not already carry the margin. Crash 3's guest can hand this owner back a value that already
carries it. A plain `retail + margin` owner would add the margin again on **every pass of that path**
and walk the centre off the representable 16.16 range.

**The rule this produced, and it is a rule rather than a special case:** a publication site whose
argument provably does not derive from CR[24]/CR[25] widens; one whose argument arrives out of the
register this owner moves passes through unchanged. Provenance per site, measured:

| call site | argument | policy |
|---|---|---|
| 0x8001892C | the constant 0 (`FUN_800188ec`, the camera setup) | widens |
| 0x80018C04 | a guest global, every frame (`FUN_80018a54`: `DAT_80068FA8 + 0x100`) | widens |
| 0x8001CFC4 | a sign-corrected halving of a table word | widens |
| 0x8001D09C | **the read-back at 0x8001CF8C** | **passes through** |

The owner also **refuses** any call site not in its measured list, so an unmeasured reach cannot be
widened by falling through to the common case. That refusal is a log line, not a default.

## The hazard: H is the near plane, and H is also a HUD scalar

H is kept in the main-RAM global **`0x80065D54`**, used by four sites (two writers, two readers)
recorded in `titles/crash3/executable.json`. **Both writers are invisible to a displacement scan** —
they are `sw $v, 0xC4($base)` through a struct pointer: `0x80017A94` (`AC6600C4`) and `0x80018924`
(`AE2200C4`). The manifest names them; the two readers `0x8001922C` and `0x80019528` are
displacement-form loads.

Two of the four sites turn H into a **gameplay-visible** decision:

- **`FUN_8003c3d0` [0x8003C3D0,0x8003C994)** rejects a vertex when `NOT (H < Z < 12000)`. The far
  limit is the word `0x24012EE0` at 0x8003C4D8, and the bound arrives as its 6th argument from
  `FUN_8001c824` at 0x8001C880. **H is the GTE near plane**, as in Crash 1 and Crash 2.
- **`FUN_80016634` [0x80016634,0x80016CE8)** computes `(H * fog * 0xAA >> 20) - 0x6C` and uses it as a
  2D overlay rectangle's Y and height, clamped to 0..216. H is a **HUD scalar** here too.

The other reader, 0x80019528 in `FUN_800192f8`, is the `H/2` GTE light-intensity term, matching Crash 2's
0x80018614 shape. The one `cfc2` H reader, 0x8003E6A0, **sits in the delay slot** of the `bltz` at
0x8003E69C, so it executes only when that branch is not taken; both paths reach `ctc2 $t7,$7` with no
branch on H. Reading it needs the delay slot or it is not read at all.

The draw area is the PSX default whole-display area — `lui $v1,0xE100` at 0x8004ECF8, stored at
0x8004ED18 — so no second clip rectangle exists to move.

## What was built

`titles/crash3/render/widescreen/crash3_widescreen.*` over the same shared rule. `Crash3Runtime::
guestWidescreenProjection()` now returns the owner instead of `nullptr`. The widening moves **OFX**,
holds **OFY** and **H**, and passes through the one measured read-back site.

`tests/crash3_widescreen.cpp` pins, at **all four** measured call sites: 4:3 identity exact on a
non-zero retail centre including a negative vertical, the 16:9 margin, idempotence, unwidening, the
pass-through, the H-holding check, the guest RAM bound, and an install proof with no HLE plan. The
pass-through is pinned as a pair: an empty pass-through list widens `0x8001D09C` by the margin, and
the measured list leaves a widened centre alone there.

## What this does not establish, and the one thing that needs a running title

**No live leg, and that limits a specific claim.** The per-call-site policy is sound against
*accumulation* — no site can carry the margin twice, and the owner's own counters report widenings and
pass-throughs separately so a run can show which path ran. What I **cannot** determine without a
running title is the **frame ordering**: whether a frame can present with only the read-back path
having published, which would show that frame at the last published centre (widened if the last
publication was a widening site, retail if it was `gte_init`'s zero). The consequence is bounded — a
transition frame, not runaway — but it is *unmeasured*, and it is named here rather than asserted
away. Closing it needs S008.

Also not established: the two `H/2` light sites and the near-plane and HUD bodies were read from
decompilation with their instruction words verified, but the **other** bound readers were not
decompiled, and the 4-site census is the honest denominator. The draw-area and translation-vector
bodies are the same shape as the siblings' but were not separately decompiled for this title.
