---
id: 13
title: Crash 1 product needs representative gameplay through Lightrec
status: investigating
symptom: The disc-backed product runs 400 frames and presents a picture, but the level's camera path never runs and no representative gameplay is reached
state_items: S011
tags: crash1,dynarec,lightrec,migration,product
created: 2026-09-04
updated: 2026-09-29
---

## Root cause

The static dispatcher and its corpus are gone. The native frame driver treated one finite Lightrec
execution budget as a complete display field, so a real run exited `BudgetExhausted` at `0x80013ADC`
inside the authenticated decompressor; it now resumes the exact returned guest PC across
positive-progress slices while preserving its CNT2 clock, and zero-progress exits are typed faults.

## Required resolution

Reach representative interactive gameplay on the authenticated `SCUS_949.00` image through psxport's
per-`Core` Lightrec executor, with Crash's native owners dispatching by complete image identity and
guest address, original calls bypassing only the current override, and every frame/host-work/interrupt
suspension an explicit executor exit. The product link audit continues to prove the gameplay binary
contains no explicit interpreter mode and no generated corpus.

Still open: the camera path does not run in a live unpaused level (issue 0024), so no gameplay state
is reached. After that, independent state/device comparison, override/original-call coverage,
executable-memory invalidation, and released-host qualification. The deleted static machinery must
not return.

Image admission is closed: psxport's `selectExecutableFile` checks size, PS-X EXE header words and
SHA-256 against `Crash1Catalog`'s identity (from `executable.json`) before `TitleSession::boot` maps
the file; `tests/crash1_catalog.cpp` pins the catalog to the manifest.
