# 0023 — Crash 1's centre leaf runs every frame; the run was never entered, the `$r31 - 4` recovery was

## The question this held

> Why does the level's camera publish no centre at all, in a live unpaused level, when the owner is
> alive and the guest has only the two writers of `CR[24]`?

## Answer: the leaf is entered once per frame, with a retail centre of (0, 0)

The premise was a **measurement**, not a fact. The reading used was a count of the log lines
`publishCentre` prints, and that line is printed **only when the latched plan is wide AND the retail
centre is non-zero**. Crash 1's per-frame centre is a camera-SHAKE word, so it is (0, 0) in normal
play and the line can never appear. The owner's own invocation count now has a run-end report
(`[crash1-boot] guest projection run-end:`), and it is not zero:

    200-frame 16:9 disc-backed run, exit 0
    guest projection run-end: centre_publications=203 pass_throughs=0 init_published=1
                              last_retail_centre=(0, 0) published_H=288
                              host_canvas=684x240 native=512x240

203 publications for 200 frames, and the third is the projection setup re-publishing on a camera-mode
change. The two publishers are exactly the two the issue listed, and the per-frame one is
**unconditional**:

    0x800123BC  jal  0x80017A14        ; in CoreLoop, reached by fall-through and by two `bne`
    ...
    0x80017F00  jal  0x80042F8C        ; inside FUN_80017A14 (GfxUpdateMatrices), straight-line
    0x80017F04  addu $a1, $a1, $v0     ; delay slot: $a1 = (DAT_80061894 >> 8) + DAT_80061940

and the decompiled C reads `FUN_80042f8c(_DAT_8006193c, (_DAT_80061894 >> 8) + _DAT_80061940);` with
no branch around it. `DAT_8006193C` and `DAT_80061940` are the camera-shake words and
`DAT_80061894` the shake decay counter: all three are 0 unless the camera is shaking, so the level's
centre is (0, 0) every frame. Ghidra's reference database agrees there are exactly **2 code
references** to `0x80042F8C`, from `FUN_80017790` and `FUN_80017A14`, and `FUN_80017A14` itself has
exactly **1** caller, `CoreLoop` at `0x800123BC`.

## The second defect this uncovered: `$r31 - 4` names a delay slot

`observedCallSite` recovered the call site as `$r31 - 4`. **MIPS sets a jump-and-link's link value to
the address after the delay slot**, so a `jal` at A leaves `$r31 = A + 8`: `$r31 - 4` is A + 4, the
delay slot. Every measured reach therefore read back four bytes past its own site, and the owner
refused the run:

    [crash1-wide:error] set_geom_offset override was reached from 0x80017840, which the title
    manifest does not name; this repository will not widen a call site whose argument provenance it
    has not measured

`0x80017840` is the **delay slot** of the `jal` at `0x8001783C` — which the manifest had recorded
correctly all along. The same repository already knew the `jal + 8` rule elsewhere: Crash 1's measured
`after_first_vsync` is `0x800170FC` for the `jal 0x8003E4F0` at `0x800170F4`. The recovery is `$r31 - 8`
now, Crash 1 is back on `ReturnAddressCallSites` with its two measured sites, and the 200-frame wide
run above exits 0 with 1,853,304 executed blocks, 20,932,382 instructions and 0 fallback blocks.

Whole-image scans settle the indirect question the issue left open as well, for all three titles:

| title | leaf | `jal` sites | resident word == leaf |
|---|---|---|---|
| SCUS_949.00 | `0x80042F8C` | `0x8001783C`, `0x80017F00` | 0 |
| SCUS_941.54 | `0x8004EFE8` | `0x800179CC`, `0x80017F70` | 0 |
| SCUS_942.44 | `0x8004F704` | `0x8001892C`, `0x80018C04`, `0x8001CFC4`, `0x8001D09C` | 0 |

Every reach of every leaf in every title is a `jal`, and no leaf address appears in any function
pointer table the images carry. Issue 0025's `IndirectCall` declaration and its mis-decode claim are
both withdrawn (see that issue).

## What IS still open, and it is not the camera

`OFX + margin` moves the centre. Beetle's `rtps` puts a projected point at `SX = OFX + H*IR1/SZ`, so
holding H and moving OFX **translates** the projection; it does not change the field of view. The
widened level frame, inspected: a live unpaused N. Sanity Beach presented on the 684-wide canvas,
1,384 polygons, `paused 0`, the guest's own 512-px composition moved right by the 86-px margin and the
added columns carrying the guest's sky fill rather than more world. Getting extra world into those
columns needs the title's own horizontal frustum — the pre-GTE camera/projection state its view matrix
carries — not the screen offset. That is CRASH1-05's open step, and it is a different owner from this
issue.

The HUD and the background fills are 2D overlays in screen coordinates, so the workspace rule "edge
HUD elements move to the widened edges" still applies to them separately.