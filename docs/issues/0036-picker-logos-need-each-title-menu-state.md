---
id: 36
title: "Picker logos need each title menu's own state"
status: open
symptom: the selector has no Crash logo; a gate on the title scene id alone read the publisher intro instead of the wordmark
state_items: S008
tags: picker,logo
created: 2026-11-03
updated: 2026-11-03
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

Next: find each title menu's own state (the menu's selection or its draw routine's mode word) with the decomp
pipeline and gate on it, then name the textures above in each title's facts.
