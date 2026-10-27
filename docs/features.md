# Features — Crash Bandicoot 1, 2 and 3

Title hops only. Each row is where the trilogy joins a psxport feature hop (`psxport F-X.n`, psxport
`docs/features.md`); a feature with no row runs on the framework alone. Shared rows are `game/`; a
title's own rows name `titles/<title>/`.

## F-BOOT

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-BOOT.1 | `product/catalog.cpp:createCatalog` — Crash 1, 2, 3 identities from each `executable.json`, one runtime each |
| 2 | psxport F-BOOT.5 | `game/runtime/runtime.cpp:Runtime::startProgram` — `Crt0Boot`, static constructors, the CD flag, `Init` |
| 3 | psxport F-BOOT.6 | `game/runtime/runtime.cpp:Runtime::registerOverrides` — `StockVSync`, `StockLibcd`, libcd init, callback init, GPU watchdog, frame seams |
| 4 | psxport F-BOOT.6 | `game/runtime/session.cpp:Session` — per-Core `FrameCut`; Crash 1 adds its block pool (`titles/crash1/entry/session.h`) |
| 5 | psxport F-BOOT.6 | `titles/crash1/entry/runtime.cpp:Runtime::registerOverrides` — Crash 1's block-pool cell lookup `0x80015978` |

## F-TURN

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-TURN.2 | `game/frame/frame_driver.cpp:FrameDriver::stepFrame` — CoreLoop iteration to the GpuUpdate return or the empty-stub transition |
| 2 | psxport F-TURN.2 | `game/frame/frame_driver.cpp:FrameDriver::deliverDisplayWait` — one field per guest VSync wait; CNT2 serviced at its `SetRCnt` target |
| 3 | psxport F-TURN.2 | `game/frame/frame_driver.cpp:FrameDriver::deliverSkippedWaits` — Crash 3's limiter skip: the fields up to the VBlank that shows the frame |

## F-INPUT

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-INPUT.1 | `game/runtime/runtime.cpp:Runtime::deliverPad` — Crash 2's libpad receive buffers |
| 2 | psxport F-INPUT.1 | `titles/crash1/frame/frame_driver.cpp:FrameDriver::publishInput` — Crash 1's BIOS `PadRead` word `0x80057054` |

## F-CD

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-CD.1 | `game/runtime/runtime.cpp:Runtime` — stock `CdControl`, `CdControlF`, `CdSync`, `CdRead`, `CdReadSync` at each title's entries |
| 2 | psxport F-CD.3 | `game/runtime/runtime.cpp:Runtime::stockCdReadLanded` — Crash 2 and 3 load level code into their heap arena |

## F-WIDE

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-WIDE.1 | `game/runtime/runtime.cpp:Runtime::guestWidescreenProjection` — the stock aspect policy; Record widens the canvas, the retail centre stays |
| 2 | psxport F-WIDE.2 | missing — the world's polygon set stops at the 4:3 edge (issue 0030) |

## F-FPS60

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-FPS60.2 | `game/runtime/runtime.cpp:Runtime::sealedFrameIsCut` — scene changes from the guest's scene words (`game/frame/frame_cut.cpp`) |
| 2 | psxport F-FPS60.1 | `titles/crash1/entry/runtime.h:Runtime::interpolates`, `titles/crash2/entry/runtime.h:Runtime::interpolates` — Crash 3 not yet |

## F-DEBUG

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-DEBUG.1 | `game/debug/debug_menu.cpp:crash::DebugMenu` — `warp <scene-hex>` through `game/frame/scene_warp.cpp:SceneWarp` |
