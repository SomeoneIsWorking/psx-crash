---
id: 29
title: "Crash 2's warp-room intro never ended in headless runs, and Start (pause) faulted on an unimplemented BIOS atoi"
status: resolved
symptom: after warp 2 the Cortex close-up looped for over 10,000 frames; a Start press in the level or hub faulted with "unimplemented BIOS A0:0x10"
state_items: S008, S009
tags: crash2,warp-room,intro,pause,bios,spu
created: 2026-10-10
updated: 2026-10-10
---

Start fault. Cause: `bios_libc_string_dispatch` (psxport `runtime/psx/hle/bios_libc_string.cpp`) had no A0:0x10
`atoi` (or A0:0x11 `atol`). The pause menu's text formatter at `0x8001BAB0` calls the libc stub at `0x800491A4`
(`li t2,0xA0; jr t2; li t1,0x10`) with `ra=0x8001BAB8`, so the fault is a real BIOS call, not a corrupted return
address. The "block began at 0x80019940" in the executor log is the last block entered, not the source of the jump.
Fix: both leaves added to the libc owner (`tests/test_bios_libc_string.cpp`). Start opens and closes the pause menu
in the hub and in level 0x1E/0x15 (shots under `scratch/crash2-0029/`).

Intro that never ends. Cause: `SpuAudio::frameEx` (psxport `runtime/psx/audio/spu_audio.cpp`) returned before
advancing the SPU whenever no device, WAV capture or XA clip consumed the output, so a headless run froze the
guest-visible voice state and the Cortex dialogue script in scene 2 waited on it forever. A windowed product always
has a device and was never affected; the same headless run with `PSXPORT_WAV` set (SPU advancing) ended the intro.
Fix: the SPU advances every field and the PCM is discarded when nothing consumes it
(`tests/test_spu_audio_headless_advance.cpp`). After `warp 2` the intro ends near frame 1,700 and Crash stands in
the hub under control.

Correction to the original report: New Game does not request scene 2. It requests scene 0x1C (Cortex cutscenes, then
a playable jungle level with Coco); the hub's Cortex intro is scene 2 itself, reached by `warp 2` or after that
level. This run did not play the 0x1C level through to the hub.

Verified headless: `warp 2`, intro ends, Start opens and closes the pause menu in the hub, forced Up walks Crash into
the Turtle Woods portal and scene 0x1E loads, Start opens and closes the pause menu there.
