---
id: 36
title: "Picker logos need each title menu's own state"
status: open
symptom: the selector has no Crash logo; a gate on the title scene id alone read the publisher intro instead of the wordmark
state_items: S008
tags: picker,logo
created: 2026-11-03
updated: 2026-11-04
---

The picker rebuilds a title's logo from its title screen's own draws (psxport `psx::present::drawnLogo`), at a
gate the title owns. Measured in a hidden selector walk (`scratch/picker2/cprobe`, record pages per step):

- Crash 1 (scene `0x19` at `0x80056710`): the sign draws the wordmark from the 4bpp pages (0..128, 256) through
  the palettes at (0, 385) and (0, 386).
- Crash 2 and 3 (scene `0x3C` at `0x8005F3EC` / `0x80060ABC`): the wordmark is drawn from the pages
  (0..192, 256); the menu text is on (640..704, 256) and Crash 3's backdrop on (832, 256).

The title scene id also holds through the Naughty Dog and Universal intros, which draw from the same pages. With
only the scene id as the gate (`scratch/picker2/crash-logo-scene-gate.diff`) the selector kept the wooden sign
without its letters (Crash 1), the intro's dark backdrop (Crash 2) and the Universal copyright card (Crash 3)
(`scratch/picker2/clogos/logos_sheet.png`); that change was not landed and the saved files were moved to
`scratch/picker2/wrong-logos`.

Third pass (`scratch/picker3`):

- RAM dumps every ~2.5 s from boot (`c1ram`, `c2ram`, `c3ram`, with sheets) hold no global word that is constant on
  the menu screens and different on every intro and demo screen for Crash 1 or 3; the differing words sit in the
  object heap. Crash 2's `0x8005FA3C` goes 0, 4, 6 and holds 6 from the finished wordmark onward, written while an
  object spawns (store watch `PSXPORT_CW`, `c2cw`, writer under `FUN_800189CC`/`FUN_80019988`); it has no static
  reference, so it is a GOOL-side word whose meaning is not yet read.
- A gate on "the frame drew from the menu-text pages" (`crash-logo-menu-text-gate.diff`, with a psxport
  `drewFrom(record, textures)`) saved Crash 1's sign without its letters, a dark intro blob for Crash 2 and Crash 3's
  clock without the wordmark (`pk1/logos.png`). So the menu-text pages are also drawn outside the menu, and the
  wordmark letters are not drawn from the pages named above: they are a 3D model textured from other pages. Not
  landed.

Fourth pass (`scratch/picker4`, `ram2` dumps with sheets, `probe2`/`probe3` record histograms):

- The title object's GOOL state marks the finished wordmark. Object fields (from the object base; layout read with
  the khasinski GOOL reference, offsets cross-checked against the RAM dumps): Crash 1 executable entry `+0x20`,
  state `+0x2C`, subtype `+0xD4`, pc `+0xE0`, child `+0x6C`, sibling `+0x68`; Crash 2 and 3 `+0x0C`, `+0x1C`,
  `+0xB4`, `+0xC0`, `+0x4C`, `+0x48`. The executable entry holds its EID at `+4` and its code item at `+0x14`;
  the code word is (pc - code item) / 4.
  - Crash 1: the player global `0x80060DB0` is GamOC (`0x5452D94D`) subtype 10 in state 7, and every GamOC
    subtype-11 child (sign and letters) has landed in its hold loop at code word `0x80D`.
  - Crash 2: TitOC (`0x6E93B94D`) subtype 1 in state 30 under the player `0x8005F38C`, only while the menu shows.
  - Crash 3: TitOC subtype 6 in state 37 under the player `0x80060A90` (35 before the menu).
- The wordmark letters are untextured Gouraud models: Crash 1 draws 1008 opaque Gouraud triangles that cover the
  letters and Crash himself, and Crash 2 and 3 draw untextured polygons over the whole screen, background
  included. Taking texture pages at the gate saved the sign, the backplate or the clock without letters
  (`logo1/logos.png`), so no texture selection gives a Crash logo.
- A gate built on these states was not landed: with no logo the picker never asks it (`GameRuntime::panelLogo`
  needs logo textures, `PanelSessions::logo` needs a held logo), so it would be dead code.

Next: attribute draws to the GOOL object that emitted them (the prim-buffer range written around an object's
render, Crash 1 `0x8001DE78`, kept per `DrawPrimitive::sourceAddress`), take the sign and letter objects'
primitives at the gate above, and land the gate with that logo selection.
