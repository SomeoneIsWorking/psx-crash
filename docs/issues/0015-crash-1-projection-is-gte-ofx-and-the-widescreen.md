# 0015 — Crash 1's projection is GTE OFX; the widescreen owner is built

**State:** owner built and gated; one framework defect blocks a wide-picture claim. **Opened:** 2026-09-27.

## What Crash 1's projection actually is

Not `H`/`OFX`/`OFY` as an authored triple, and not a viewport rectangle. It is the **GTE screen offset
`OFX`, cop2 control register 24**, and the retail value is **zero**.

`RTPS` computes `h_div_sz = Divide(H, Z_FIFO(3))` then `TransformXY(h_div_sz, ...)`, so a projected
point lands at `SX = OFX + (H * IR1) / SZ` — `OFX` is the pixel centre and `H` is the scale. Crash 1
publishes `OFX = 0`, and the centring that makes the picture sit inside the display lives in the
title's own 3x3 view matrix, published into the GTE translation vector through `SetGeomTranslation`
`0x80042C2C`.

### Control-register census over all 72192 instruction words

Ranges are recorded in `titles/crash1/executable.json` (`runtime.projection`).

| GTE control register | writers | readers |
|---|---|---|
| `CR[24]` OFX | **2** — `0x80042B88` (gte_init), `0x80042F94` (`SetGeomOffset`) | **0** |
| `CR[25]` OFY | **2** — `0x80042B8C`, `0x80042F98` | **0** |
| `CR[26]` H | **2** — `0x80042B68`, `0x80042FAC` (`SetGeomScreen`) | **0** |

**The zero readers are load-bearing.** No guest branch tests `OFX`, `OFY` or `H`, so moving `OFX`
cannot flip a decision the title makes. The three `mfc2 $x,0xC000` sites at `0x800347B4`,
`0x80034CF0` and `0x80035194` look like `OFX` readers and are not: they are `mfc2` **data** reads of
GTE data register 24, a reserved slot that always reads zero.

### Retail 4:3 values

- `gte_init` `0x80042B1C`: **`H = 0x3E8` (1000), `OFX = 0`, `OFY = 0`**, `ZSF3 = 0x155`,
  `ZSF4 = 0x100`, `DQA = 0xEF9E`, `DQB = 0x01400000`. One `jal` call site, `0x80016558`, inside
  `Init` `0x8001652C`.
- `FUN_80017790` republishes `H` per camera mode from `*(int *)(DAT_8005C53C + 0x114)`:
  `0x25 -> 500`, `0x1E -> 960`, `0x38 -> 800`, `0x3C -> 460`, `0x5A -> 288`, into the global
  `DAT_800578D0`, and calls `SetGeomOffset(0, 0)` alongside it (`0x8001783C`).
- `FUN_80017A14` calls `SetGeomScreen(DAT_800578D0)` at `0x80026770` **every frame** and
  `SetGeomOffset(DAT_8006193C, (DAT_80061894 >> 8) + DAT_80061940)` at `0x80017F00` **every frame**.
  `DAT_8006193C` is a live camera-shake word, so the centre is not a constant.

### The draw area

The guest writes GP1 `0xE1` in exactly one place, `FUN_80042A04` at `0x80042A58`, with
`(GPUSTAT & 0x3FFF) | 0xE1001000` — origin `(0, 4)`, zero width and height, which on a PSX **is the
whole display area**. There is no separate clip rectangle, so the single horizontal extent this title
publishes is the display extent it sends through GP1 `0xC0`.

That publisher, `FUN_80041C38`, has **zero direct `jal`/`j` call sites**. It is entry index 7 of the
GPU driver function-pointer table at `0x80054A24` (18 guest-code words), so an address-target scan
cannot reach it; a resident-word scan finds it immediately.

### Culling

No immediate-bearing compare against a 4:3 dot width exists: 320 appears only in two `addiu $sp`
stack frames, 319/318/352/368 appear zero times, and the single `0xBFFF` visibility-mask site at
`0x80016824` is the active-low digital pad button mask. **A cull against a variable bound carries no
immediate and is invisible to that argument**; the variable bound in this title is the main-RAM global
`0x800578D0`, whose twenty readers and two decision-making consumers are recorded in
`titles/crash1/executable.json` and owned by `crash1_horizontal_bound.*`. Because the widening holds
`H` fixed, all twenty readers keep reading a retail `H`.

Two reading traps this title reproduces: there is no gp-relative addressing (globals are
`lui $r,0x8005` + a 16-bit offset, so a gp-relative reference scan returns zero for every global),
and `ctc2` writes control register `<cop2op> bits 11..15` with the transfer class in bits 21..25,
not in the immediate's low five bits.

## What was built

`titles/crash1/render/widescreen/crash1_widescreen.{h,cpp}`, wired from `Crash1Runtime::registerOverrides` and
returned from `Crash1Runtime::guestWidescreenProjection()`. The widening is
**`OFX' = retail_OFX + projectionHorizontalMargin`, `OFY' = retail_OFY`, `H' = retail_H`**. A
428-wide canvas holding the same 4:3 frame at its original pixel scale is the same equation with
`OFX = (428-320)/2 = 54` and `H` unchanged; substituting a smaller `H` would be a zoom, and this
owner never touches `H` — `SetGeomScreen` is overridden only to record what the guest published.

- **No per-frame host re-publish:** the guest itself calls `SetGeomOffset` every frame.
- **Idempotent by construction:** the base is `$a0` as it stands now and the coprocessor register is
  never fed back into it. A captured baseline is wrong, because `DAT_8006193C` is live.
- **4:3 identity is exact by construction:** the margin is zero.

## Still open — a framework defect, not a title one

`runtime/psx/picture_announce.cpp:69` judges a widening with
`classifyWide(aspect, core.rsub.mode.enhancementsAllowed(), ...)`, and `enhancementsAllowed()` is
`mPath == RenderPath::Native`. A `widescreenOnly` title's path is **Gte**, so the classifier always
returns `RefusedPure` and logs `any widescreen claim from this run is void` for a **guest** widening,
which the presentation contract deliberately allows on Gte. The gate itself is correct; using it to
judge a guest widening can never be true on Gte. Flipping Crash 1 to `RenderPath::Native` to silence
it would hand the title an enhancement path it does not have, so it is reported and left.

Related: `gpu_vk.cpp:342` widens the host picture to `plan().presentationExtent.width` while the
guest's own GP1 display extent stays 320, so whether the guest rasterizer honours the host canvas
width is an open framework display/present-policy question. This repository does not edit
`external/psxport`.
