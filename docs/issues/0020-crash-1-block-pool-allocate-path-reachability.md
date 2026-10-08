---
id: 20
title: The size-class block pool: the fault is fixed, the allocate path's reachability is not established
status: investigating
symptom: The disc-backed product runs to its display wait and presents a picture, but nothing establishes that the guest ever reaches the pool's allocate path at 0x80012F10
state_items: S006,S011
tags: crash1,re,block-pool,native-override
created: 2026-09-28
updated: 2026-09-29
---

## The fault, and what it was

Guest execution left frame 0 at `0x800159A8` with reason `Fault` after 8,177,050 cycles — not a
budget exit, and not a translation refusal (`fallback_blocks=0` in the same log). The guest printed its
own diagnosis first:

    ERROR: Segmentation fault in recompiled code: invalid load/store at address PC 0x00800004

`0x800159A8` is the word `0x8C620004` — `lw $v0,0x4($v1)`, the second class read of the engine's
size-class cell lookup whose entry is `0x80015978`. The pointer it followed was `0x00800000`, unmapped.

The lookup owner `titles/crash1/render/crash1_block_pool.*` had recovered the class as
`request >> 13`, from the entry's `srl $v0,$a0,13`. **The image compares the request word WHOLE**:
`beq $2,$4` at `0x8001599C` and `bne $2,$4` at `0x800159B0` test `$a0`, which nothing between the entry
and them redefines; the bounded sibling at `0x800159E8` does the same, and the allocate path writes and
compares the unshifted word at `0x80012FFC`. So the owner could never match a cell the engine wrote:
every walk ran off the end of main RAM. Controlled pair, same binary and disc, 400 frames: unservable
classes 5 → 0, executed blocks 1,689,262 → 3,461,249, executed instructions 26,457,241 → 46,438,388,
`fallback_blocks` 0 in both.

The owner's walk bounds were also corrected against the image: the engine's own bound at `0x800159C4`
is read, evaluated and reported but does not stop the walk, because enforcing it was measured to break
a path retail completes. The walk stops only at the edge of main RAM.

## What is still open

1. **The allocate path's reachability.** `0x80012F10` reads the same bucket table at `0x80012FC4` and
   is the function that FILLS a cell: a 44-byte node table at `0x8005C554`, its cursor at `0x8005CAF`,
   a five-way kind dispatch on the node's `+0x02` halfword, a cell link at `0x80013020`, the `+0x0A`
   live counter, and a 28-byte-stride per-type callback table at `0x800514EC`. It is a readable, tested
   model (`titles/crash1/render/crash1_pool_node.{h,cpp}`) and is deliberately **not** installed as an
   override, because nothing establishes that the guest reaches it: every lookup in the disc-backed
   run came from `ra = 0x80015164` inside `FUN_80015118`, never from `0x80013140`. The next step is a
   reachability measurement, not a transcription.
2. **A framework defect, named rather than worked around.** `OtAttr::poolRangeMiss` runs only under
   `c->cfg != mPoolCfg` with `mPoolCfg` initialised to `nullptr`, so a typed `GameRuntime` with no
   declared packet-pool window never resolves its window and never reaches the framework's own
   "not measured" warning — `runtime/psx/native_boot.cpp:286` prints a bare `0` instead. This
   repository does not edit `external/psxport`.

## Not established

- **Any drawn aspect.** The product presents a picture and every `[wide]` line still reads
  `render_width == native_width`, so no widening claim is made from these runs.
- **The unit of the request word.** The callers do not pass a uniform byte count — `0x80015118` passes
  a tagged handle read from a struct at `lw a0,0(s0)`. The measured requests are `0x0057CCFB`,
  `0x15814CE7`, `0x5452D94D`, `0x4E938CCD`; none is a plausible byte count, which is consistent with a
  handle and does not establish it.
- **That a direct-call census is the whole call graph.** One class per bucket is this implementation's
  reading of a linear 8-byte-stride walk; a switch table or function pointer would not appear in it.
