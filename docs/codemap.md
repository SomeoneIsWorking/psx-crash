# Codemap

Where code lives in the Crash trilogy port. Capability state is in `docs/project-state.md`, feature hops in
`docs/features.md`, the evidence chain, symbol map and RE notes in each title's decomp (`external/crash-bandicoot-decomp`, `external/crash2-decomp`, `external/crash3-decomp`), atomic work in `docs/issues/`. A title's
measured executable facts are in `titles/<title>/executable.json` and its `entry/facts.cpp`.

Product path: `run.sh` → the framework main (`psxport_add_product`, target `crash_port`) →
`psx::host::ProductHost` over `crash::Catalog` (in-window picker; `pick crash1|crash2|crash3`) →
`TitleSession` → the title's `Runtime` → `crash::FrameDriver::stepFrame` on psxport's Lightrec executor.

Frame loop: not owned. Each step runs the core loop iteration (`native_frame.iteration` in each `executable.json`) to GpuUpdate's VSync waits, one field per wait (issue 0031).

## Placement rules

- `titles/<title>/` holds what is measured out of one executable, in namespace `crash1`/`crash2`/`crash3`.
- `game/` holds behaviour proven common across the binaries (matching instructions or body hashes), in
  namespace `crash`. A title differs from the shared owner by subclass and virtual override only.
- Includes name the owning directory: `"runtime/runtime.h"` under `game/`, `"crash1/entry/runtime.h"` under
  `titles/`; psxport headers are bare.
- psxport (`external/psxport`, a link to the live checkout) owns the mechanism: the executor, override keys,
  stock libcd/VSync/pad classes, `Crt0Boot`, presentation. A title supplies addresses.

## Owners

| Owner | Path | What it owns |
|---|---|---|
| `crash::Runtime` | `game/runtime/runtime.*` | The shared `GameRuntime`: crt0 and C main's boot steps, stock `StockVSync`/`StockLibcd`/`StockPadBuffers` at the title's addresses, the boot and frame overrides, code-module landings, Record, the debug menu |
| `crash::TitleFacts`, `isCoherent` | `game/runtime/title_facts.h` | Every address the shared owners need from one executable, and the compile-time check each facts file asserts |
| `crash::Session`, `session(core)` | `game/runtime/session.*` | Per-Core state outside guest RAM: the `FrameCut` |
| `crash::libcd_init` | `game/boot/libcd_init.*` | Native libcd initialisation over the title's libcd state words |
| `crash::callback_boot` | `game/boot/callback_boot.*` | Native callback/event initializer; each BIOS leaf runs as guest code with its retail return address |
| `crash::gpu_watchdog` | `game/boot/gpu_watchdog.*` | libgpu queue-timeout bookkeeping on the host display counter (Crash 1 and 2; Crash 3's check differs and runs as guest code) |
| `crash::LeafCall`, `callLeaf` | `game/boot/leaf_call.h` | One guest leaf call from a native boot owner, at a return address inside the retail body |
| `crash::FrameDriver` | `game/frame/frame_driver.*` | One host frame: CoreLoop iteration to the transition, one display field per guest VSync wait, CNT2 at its `SetRCnt` target, presentation commit; the perf meter's logic frame (game-logic, present) |
| `crash::FrameProgram`, `SceneTable` | `game/frame/frame_program.h` | A title's measured frame loop: CoreLoop, iteration, transition, GpuUpdate VSync sites, the loading swap, scene words, root-counter entries |
| `crash::FrameCut` | `game/frame/frame_cut.*` | Whether a sealed frame is a cut: a new scene from the guest scene id and request words, or a cut the title declared |
| `crash::DebugMenu` | `game/debug/debug_menu.*` | The title debug menu (psxport `TitleDebugMenu`): the warp catalogue (`scene 0xNN`) over `SceneWarp`, refused until the game loop runs |
| `crash::SceneWarp` | `game/frame/scene_warp.*` | The scene arrival: arms a scene id and writes the request word at a frame boundary; refused before the loop runs, past 0x3C, or on an empty scene-table row |
| `crash1::Runtime`, `crash1::facts` | `titles/crash1/entry/` | Crash 1's facts, its session (`crash1::Session` with the block pool) and frame driver |
| `crash1::FrameDriver` | `titles/crash1/frame/frame_driver.*` | The shared turn plus the BIOS `PadRead` word publication |
| `crash1::bios_pad_input` | `titles/crash1/input/bios_pad_input.*` | Crash 1's BIOS pad word `0x80057054` and Crash's byte order |
| `crash1::BlockPool` | `titles/crash1/render/block_pool.*` | The size-class cell lookup at `0x80015978` and its walk (issue 0020) |
| `crash2::Runtime`, `crash2::facts` | `titles/crash2/entry/` | Crash 2's facts: libpad buffers, code-module arena, scene table; its session (`crash2::Session` with the draw replay) |
| `crash2::DrawSection` | `titles/crash2/frame/draw_section.*` | The replayed picture half of `Sys_MainLoop`: `Gfx_UpdateView` to `Gfx_EndFrame`'s `Snd_Update`, objects queued by `Obj_UpdateAll` with `g_FrameFlags` 0x8 (object logic) clear (crash2-decomp `docs/notes/frame-loop.md`) |
| `crash2::DrawReplay` | `titles/crash2/frame/draw_replay.*` | Crash 2's in-betweens: psxport `ReplayFrame` over the camera, object-pool and player tracks (position, angles, scale stepping on a flip), sampled at `Sys_BeforeDrawObjects` |
| `crash2::ViewEntry` | `titles/crash2/frame/view_entry.*` | What `Gfx_UpdateView` steps (screen shake, streak scroll), recorded at each real call and written blended into a replay |
| `crash2::pose_blend` | `titles/crash2/frame/pose_blend.*` | An object's pose in a t = 1/2 replay, at `Gfx_QueueObject`: its two ticks' vertex frames refetched (`func_8001A4A4`); when the four are at most two frames, that pair is queued with k quarters on the second, keyed by the queued packet |
| `crash2::MorphReader` | `titles/crash2/render/morph_reader.*` | The morph vertex readers `func_8003ED14` and `func_80040D14` in host code with k quarters on the second frame (k = 2 is the retail sum), and the five morph draw steps, which set a replay packet's k and its box at the same weight |
| `crash2::camera_cuts` | `titles/crash2/frame/camera_cuts.*` | The game's camera cuts (prepared cut, zone walk, checkpoint restore) declared to `crash::FrameCut` |
| `crash3::Runtime`, `crash3::facts` | `titles/crash3/entry/` | Crash 3's facts: no GPU watchdog override, libpad via the title's own SIO, code-module arena, scene table |
| `crash::Catalog`, `createCatalog` | `product/catalog.*` | The product's three identities and runtimes, provisioned under `scratch/bin` |
| tools | `tools/` | Provisioning, executable verification, launcher, the verifier |

## The frame turn

| Hop | Owner |
|---|---|
| product spine | `ProductHost` → `TitleSession`: `admitExecutable` → `Runtime::registerOverrides` → `startProgram` |
| one host frame | `crash::FrameDriver::stepFrame`: first frame enters CoreLoop with the initial scene, later frames resume at `iteration.begin` |
| boundary | the guest's VSync call returning to a measured GpuUpdate site or to Crash 1's loading swap (`FrameProgram::loadingSwap`), or the transition override at the GpuUpdate return (Crash 1 and 3) or the empty stub after it (Crash 2) |
| skipped waits | Crash 3's GpuUpdate limiter (`FrameProgram::limiter`) skips both waits; `deliverSkippedWaits` delivers the fields up to the next VBlank, any other wait-less frame aborts |
| per display field | `deliverField` → CNT2 service → `SpuAudio::frame` |
| presentation | psxport `FramePresenter::commit`; Record widens the canvas at 16:9 and keeps the retail centre |

## Input, CD, widescreen

| Concern | Owner |
|---|---|
| host pad | psxport `Pad`, serviced by `LogicFrame::begin` at the top of `stepFrame` |
| Crash 1 pad | `crash1::FrameDriver::publishInput` before retail `PadUpdate` |
| Crash 2 pad | `crash::Runtime::deliverPad` → `StockPadBuffers` |
| Crash 3 pad | the title's own libpad SIO path over psxport's SIO device |
| stock libcd | `StockLibcd` bound in `crash::Runtime::registerOverrides`; CD reads are synchronous |
| level code | `crash::Runtime::stockCdReadLanded` → `publishStockReadLanding` over the title's arena |
| widescreen | `crash::Runtime::guestWidescreenProjection` (stock aspect policy); the guest's own horizontal culls are not yet widened (issue 0030) |

## Where does it go?

| Responsibility | Owner |
|---|---|
| A new measured address for every title | a `TitleFacts` field, filled in each `titles/<t>/entry/facts.cpp`, from `executable.json` |
| Behaviour only one title has | that title's `Runtime` override or session subclass under `titles/<t>/` |
| A native override | `psx::cpu::installNativeOverride` from the owning module's `registerOverride(s)` |
| Executor, presentation, stock library mechanics | psxport |
| Capability, measured facts, blockers | `docs/project-state.md`, `executable.json`, `docs/issues/` |
