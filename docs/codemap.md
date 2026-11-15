# Codemap

Where code lives in the Crash trilogy port. Capability state is in `docs/project-state.md`, feature hops in
`docs/features.md`, the evidence chain, symbol map and RE notes in each title's decomp (`external/crash-bandicoot-decomp`, `external/crash2-decomp`, `external/crash3-decomp`), atomic work in `docs/issues/`. A title's
measured executable facts are in `titles/<title>/executable.json` and its `entry/facts.cpp`.

Product path: `run.sh` → the framework main (`psxport_add_product`, target `crash_port`) →
`psx::host::ProductHost` over `crash::Catalog` (in-window picker; `pick crash1|crash2|crash3`) →
`TitleSession` → the title's `Runtime` → `crash::FrameDriver::stepFrame` on psxport's Lightrec executor.

Frame loop: not owned. Each step runs the core loop iteration (`native_frame.iteration` in each `executable.json`) to GpuUpdate's VSync waits; each wait delivers `Timing::vsyncFields(0)` fields with their VBlanks there (issue 0031).

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
| `crash::FrameDriver` | `game/frame/frame_driver.*` | One host frame: CoreLoop iteration to the transition, each guest VSync wait's fields (`Timing::vsyncFields`) delivered at the wait with their VBlanks, CNT2 at its `SetRCnt` target, presentation commit; the perf meter's logic frame (game-logic, present) |
| `crash::FrameProgram`, `SceneTable` | `game/frame/frame_program.h` | A title's measured frame loop: CoreLoop, iteration, transition, GpuUpdate VSync sites, the loading swap, scene words, root-counter entries |
| `crash::FrameCut` | `game/frame/frame_cut.*` | Whether a sealed frame is a cut: a new scene from the guest scene id and request words, or a cut the title declared |
| `crash::DebugMenu` | `game/debug/debug_menu.*` | The title debug menu (psxport `TitleDebugMenu`): the warp catalogue (`scene 0xNN`) and `spawn <n>` over `SceneWarp`, refused until the game loop runs |
| `crash::SceneWarp` | `game/frame/scene_warp.*` | The scene arrival: arms a scene id (and for `spawn`, the current scene at one of the level's spawn points, `FrameProgram::spawns`) and writes the request word at a frame boundary; refused before the loop runs, past 0x3C, on an empty scene-table row, past the level's spawn count, in the warp room (it picks its own spawn) and at a spawn the game never starts the level at (`SpawnPoints::carriedSpawns`) |
| `crash1::Runtime`, `crash1::facts` | `titles/crash1/entry/` | Crash 1's facts, its session (`crash1::Session` with the block pool) and frame driver |
| `crash1::FrameDriver` | `titles/crash1/frame/frame_driver.*` | The shared turn plus the BIOS `PadRead` word publication |
| `crash1::bios_pad_input` | `titles/crash1/input/bios_pad_input.*` | Crash 1's BIOS pad word `0x80057054` and Crash's byte order |
| `crash1::BlockPool` | `titles/crash1/render/block_pool.*` | The size-class cell lookup at `0x80015978` and its walk (issue 0020) |
| `crash1::ObjectCull` | `titles/crash1/render/object_cull.*` | The screen test of `Gfx_CalcObjectMatrices` 0x800180CC in host code, its columns over the record canvas's (crash-bandicoot-decomp `docs/notes/screen-culls.md`); the original body resumes after it. An object only the canvas keeps is drawn by its caller in a `GuestRedraw` keeping only its packets, off the guest clock. Crash 1's world renderer has no screen test |
| `crash2::Runtime`, `crash2::facts` | `titles/crash2/entry/` | Crash 2's facts: libpad buffers, code-module arena, scene table; its session (`crash2::Session` with the draw replay) |
| `crash1::DrawSection` | `titles/crash1/frame/draw_section.*` | The replayed picture half of `CORE_Loop`: the render buffer's ordering table cleared and its primitive cursor reset as `Gpu_Update` leaves them, then `Gfx_UpdateMatrices` through the worlds and `Gool_UpdateObjects` (with `g_cur_lid_ro` 0x8, object logic, clear) to `Gpu_Update`'s `Audio_Update` |
| `crash1::DrawReplay` | `titles/crash1/frame/draw_replay.*` | Crash 1's in-betweens: psxport `ReplayFrame` over `crash::ReplayTracks`, sampled at `Gool_UpdateObjects`' return to `CORE_Loop`; a replay skips `Pad_Update` |
| `crash1::PoseBlend` | `titles/crash1/frame/pose_blend.*` | A vertex-animated object's pose in a replay, at `Gfx_TransformSvtx`: the frame each object drew in the last real pass, blended t of the way to the replay's frame (origin, box and collision point linearly, position bytes unsigned, normal bytes signed) and written over that frame for the one call; a different model or vertex count draws the later frame |
| `crash1::ScreenEffects` | `titles/crash1/frame/screen_effects.*` | The screen shake and the fade overlay in a replay: `Gfx_UpdateMatrices` gets back the shake the newest tick's real call read (that call stepped it for the tick after), and `Gpu_Update`'s `Gpu_DrawOverlay` draws its fade t of the way from the last real tick's overlay |
| `crash1::frame_facts` | `titles/crash1/frame/frame_facts.h` | `CORE_Loop`'s call sites, the render buffer, frame flags, camera and object pool layout the replay reads |
| `crash2::PoseBlend` | `titles/crash2/frame/pose_blend.*` | Crash 2's `crash::QueuePoses`: an object's pose in a t = 1/2 replay, at `Gfx_QueueObject`: its two ticks' vertex frames refetched (`func_8001A4A4`); when the four are at most two frames, that pair is queued with k quarters on the second; three or four frames queue the pair that differs with the other beside it, a quarter on each frame; keyed by the queued packet |
| `crash2::MorphReader` | `titles/crash2/render/morph_reader.*` | The morph vertex reader `func_8003ED14` in host code with quarter weights on the packet's two frames and on a second pair of the same model read in step beside them (`func_80043658` opens both; 2 and 2 on the packet's pair is the retail sum), charging the retail path's instructions to the guest clock (the lit reader `func_80040D14` stays guest code, plan 0002), and the five morph draw steps, which set a replay packet's weights and its box at the same weights |
| `crash::ReplayTracks` | `game/replay/replay_tracks.*` | The camera, object-pool and player tracks every Crash replay blends (position and angles by motion, a scale stepping on a flip, the animation stepping), from a title's `TrackFacts` |
| `crash::DrawReplay` | `game/replay/draw_replay.*` | Crash 2 and 3's in-betweens over a title's `ReplayFacts` (`TitleFacts::replay`): psxport `ReplayFrame` over the tracks, the view entry and the fade, sampled at `Sys_BeforeDrawObjects`; owned by `crash::Session` |
| `crash::DrawSection` | `game/replay/draw_section.*` | The replayed picture half of `Sys_MainLoop`: `Gfx_ResetFrame`, `Gfx_UpdateView` to the end of frame's sound update, objects queued by `Obj_UpdateAll` with `g_FrameFlags` 0x8 (object logic) clear, in the real walk's order (crash2-decomp `docs/notes/frame-loop.md`) |
| `crash::FadeLevel` | `game/replay/fade_level.*` | The screen fade in a replay: `Gfx_DrawFade`'s quad at the grey t of the way between the two ticks' quads (the end of frame steps the fade word before it draws it) |
| `crash::ObjectWalk` | `game/replay/object_walk.*` | The objects the real `Obj_UpdateAll` queued, in its walk's order; a replay's `Obj_ForEachList` runs `Obj_Update` on them instead of walking the lists the logic left; `Gfx_QueueObject` hands each queue to the title's `QueuePoses` |
| `crash::ViewEntry` | `game/replay/view_entry.*` | What `Gfx_UpdateView` steps (screen shake, a title's streak scroll), recorded at each real call and written into a replay |
| `crash::world` | `game/render/world_*` | The world renderer (Crash 2 `Gfx_DrawWorlds` 0x80041E5C, Crash 3 0x80040ED8) in host code, one implementation over a title's `world::Title` (entry, guest calls, frame and scene layout, vertex scale, depth formula, link tails, retail instruction counts): the dispatcher (`world_renderer`), the polygon's state, texture lookup and link tails (`world_pass`), the default projectors and drawers (`world_default`), the modes both titles have, sway, pulse and glow, over the title's `SwayFacts`/`PulseFacts`/`GlowFacts` (`world_modes`); the packed screen test over the record canvas's columns (`recordCanvasColumns`) and the ordering table's head link, shared with the other world drawers (`world_screen`); charges a margin-only polygon as the retail cull did |
| `crash2::WorldRenderer` | `titles/crash2/render/world_*` | Crash 2's `world::Title` and registration (`world_renderer`), its own colour and vertex modes (`world_modes`) and the light around Crash (`world_light`) |
| `crash3::WorldRenderer` | `titles/crash3/render/world_*` | Crash 3's `world::Title` and registration (`world_renderer`) and its underwater mode (`world_underwater`) |
| `crash3::WaterGrid` | `titles/crash3/render/water_grid.*` | The jet-ski levels' sea, the water grid 0x80043FFC with its row, vertex and cell routines, in host code (crash3-decomp `docs/notes/water-grid.md`): the retail walk, charged to the guest clock, then at 16:9 a walk over the canvas's columns (`world_screen`) that draws only the cells the retail test culls, with the scratchpad and GTE put back |
| `crash2::camera_cuts` | `titles/crash2/frame/camera_cuts.*` | The game's camera cuts (prepared cut, zone walk, checkpoint restore) declared to `crash::FrameCut` |
| `crash3::camera_cuts` | `titles/crash3/frame/camera_cuts.*` | The game's checkpoint restore (`Sys_LoadCheckpoint` 0x80030388) declared to `crash::FrameCut` |
| `crash3::Runtime`, `crash3::facts` | `titles/crash3/entry/` | Crash 3's facts: no GPU watchdog override, libpad via the title's own SIO, code-module arena, scene table, the draw replay's `ReplayFacts` |
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
| widescreen | `crash::Runtime::guestWidescreenProjection` (stock aspect policy); the world's column cull is widened in `crash::world` (Crash 2 and 3); Crash 3's water grid in `crash3::WaterGrid`; not yet widened: Crash 1's world and the object culls (Crash 2 box test 0x80041D14, Crash 3 box test `D_8003DEF4` and model drawers; crash2-decomp and crash3-decomp `docs/notes/screen-culls.md`, issue 0030) |

## Where does it go?

| Responsibility | Owner |
|---|---|
| A new measured address for every title | a `TitleFacts` field, filled in each `titles/<t>/entry/facts.cpp`, from `executable.json` |
| Behaviour only one title has | that title's `Runtime` override or session subclass under `titles/<t>/` |
| A native override | `psx::cpu::installNativeOverride` from the owning module's `registerOverride(s)` |
| Executor, presentation, stock library mechanics | psxport |
| Capability, measured facts, blockers | `docs/project-state.md`, `executable.json`, `docs/issues/` |
