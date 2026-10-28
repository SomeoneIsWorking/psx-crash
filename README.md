# Crash

Native PC ports of the North American Crash Bandicoot trilogy, built on
[psxport](https://github.com/SomeoneIsWorking/psxport). Each product combines title-owned native
subsystems with psxport's pinned Lightrec executor for the retail code that remains guest owned.

## Screenshots

Captured from the port at 4:3 with the internal resolution raised. Widescreen still leaves the
margins partly drawn (issue 0030) and 60 fps does not interpolate yet (issue 0032), so neither is
shown.

| Crash Bandicoot | Crash Bandicoot 2 | Crash Bandicoot 3 |
|---|---|---|
| ![Crash runs a jungle path beside a warthog](docs/media/crash1-jungle.png) | ![Crash faces Tiny on the boss arena's platforms](docs/media/crash2-tiny.png) | ![Coco on the Great Wall at night](docs/media/crash3-great-wall.png) |
| ![Crash at a rope bridge by a Danger sign](docs/media/crash1-bridge.png) | ![Crash meets the Komodo Bros. in their arena](docs/media/crash2-komodo-bros.png) | ![Crash on his motorbike at a night race start](docs/media/crash3-motorbike.png) |
| ![Crash fights Papu Papu in his hut](docs/media/crash1-papu-papu.png) | ![Crash rides the jet board down a jungle river (attract demo)](docs/media/crash2-river.png) | ![The N. Gin mech battle on the moon](docs/media/crash3-n-gin.png) |
| | ![Crash in a warp room](docs/media/crash2-warp-room.png) | |

The repository is one engine lineage with title-specific integration under `titles/crash1/`,
`titles/crash2/`, and `titles/crash3/`. Crash Bandicoot is the active title; Crash 2 and Crash 3 do
not begin execution migration until Crash 1 passes representative gameplay.

## Current evidence

- Crash 1 (`SCUS_949.00`), Crash 2 (`SCUS_941.54`), and Crash 3 (`SCUS_942.44`) have verified USA
  executable and disc-selection facts. Crash 3 explicitly rejects its disc's unrelated
  `DRAGON/SPYRO.EXE` as the boot target.
- Independent CPU evidence reaches the recorded resident boundaries in all three titles; the exact
  addresses, hashes, syscall state, and VSync bodies live in `titles/<title>/executable.json`, and
  `docs/re-frontier.md` records what has and has not been reverse-engineered.
- The Crash 1 product runs the real disc: it boots, submits its own GP0 traffic, presents frames, and
  takes input, reaching a real level ("N. Sanity Beach") through Start and the menu. Live traces
  ground `GfxUpdateMatrices` at `0x80017A14` and `GoolObjectTransform` at `0x8001DE78` as pre-GTE
  ownership candidates.
- Host input reaches Crash's BIOS auto-pad word at `0x80057054`, published by
  `titles/crash1/input/bios_pad_input.*` before retail `PadUpdate`.
- Crash 2 and Crash 3 boot on the same shared runtime and reach levels through the `warp` debug command.

Reaching a rendered level is not representative gameplay, and none of this proves a native renderer
or interpolation. At 16:9 the Record canvas widens, but the guest's own culls do not yet (issue 0030).

## Product and migration contract

The target `run.sh` path provisions the user's disc, validates its exact title identity, builds the
native/Lightrec product, and launches it without offline guest-code emission. The product is
psxport's multi-title host (`psx::host::ProductHost` over `crash::Catalog`, through the framework
main): zero arguments open the in-window title picker over Crash 1, 2 and 3; one executable argument
runs that title directly. There is no full-game
interpreter or player-selectable interpreter mode. Lightrec may use only its bounded, measured
per-block fallback for explicit backend reasons; the independent interpreter remains a separately
built diagnostic oracle.

The launcher now targets the native/Lightrec composition. The former translator, generated corpus,
static dispatcher, seed inputs, and generated-symbol tests were removed before integration. Follow
`docs/migration.md`: finish the shared executor boundary, reproduce the current menu/PadRead
frontier, then prove representative interactive gameplay.

The game media, extracted executable, traces, and runtime cache remain untracked. Player media
resolution remains explicit argument, title/generic environment or `.env`, then one unambiguous
repository-root CHD. Incorrect or conflicting inputs refuse without replacing a valid selection.

## Native enhancements

Native rendering consumes game-owned camera, transform, material, primitive, and ordering state
before GTE/OT/GP0 submission. Widescreen widens the owned projection and viewport without stretching
the final image. Interpolation retains authoritative previous/current simulation transforms and
decorates presentation only. These enhancements remain off during faithful oracle comparison.

See `docs/project-state.md` for factual coverage, `docs/project-goals.md` for completion conditions,
`docs/codemap.md` for ownership, and `docs/re-frontier.md` for the ordered RE dependencies.

## Building

Maintainers select Clang and build explicitly; `run.sh` remains the player launcher:

```sh
CC=clang CXX=clang++ uv run --frozen python external/psxport/tools/port/consumer_build.py --build build/agent --target crash_port
```

For explicitly provisioned runtime dependencies, set `PSXPORT_LIGHTREC_DIR` to the maintained
Lightrec checkout and `PSXPORT_LIGHTNING_PREFIX` to the maintained GNU Lightning install prefix.
The player launcher and verifier use the same shared dependency validation and CMake arguments.

Hosted CI exercises the real asset-free native/Lightrec boundary on Linux x86-64. Windows x86-64,
Apple Silicon macOS, and Android arm64-v8a remain explicit missing host qualifications rather than
platform-named policy checks; see the host qualification details in `docs/project-state.md`.
