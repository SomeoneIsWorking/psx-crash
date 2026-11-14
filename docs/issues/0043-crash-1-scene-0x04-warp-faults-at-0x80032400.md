---
id: 43
title: "Crash 1: a warp to scene 0x04 faults in guest code at 0x80032400"
status: open
symptom: the product aborts (exit 139) about a second after `warp scene 0x04`, from the title or from scene 0x03
state_items: S011
tags: crash1,crash,warp
created: 2026-11-14
updated: 2026-11-14
---
Reproduce: `scratch/c1cull/diffrun.py <tag> 04` (boot, `warp scene 0x04` once the loop runs, 6 s), or `03 04`. The log's
last line is `[crash-frame:error] frame N left guest execution at 0x80032400 with fault ... (Lightrec execution fault)`,
then exit 139, on every run. It happens with `crash1::ObjectCull` unregistered too (2026-11-14, `03 04 05`): not that
owner's. 0x80032400 is inside `Title_Loading` (0x800322CC, 130 words, the loading splash; crash-bandicoot-decomp
`docs/names.md`). Every other scene in the
warp catalogue loads (0x03, 0x05..0x39, 2026-11-14). Not traced.
