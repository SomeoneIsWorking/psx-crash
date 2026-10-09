# Crash trilogy port agent instructions

This repository targets one native PC product per title, with title-owned native subsystems and
psxport's pinned Lightrec integration executing every remaining retail instruction at runtime.
Read `docs/migration.md`, `docs/project-state.md`, `docs/codemap.md`, and `docs/re-frontier.md`
before implementation; the frontier records what has and has not been reverse-engineered. The
workspace rules in `../AGENTS.md` and framework-consumer rules in `external/psxport/AGENTS.md` also
apply at the shared runtime boundary.

## Execution contract

- The gameplay products consume the user's identity-verified executable directly through the
  per-`Core` psxport-Lightrec executor. They never emit, compile, link, or select generated guest
  source.
- No full-game interpreter or player-selectable interpreter mode may exist. Lightrec remains the
  mandatory gameplay backend; its internal, bounded block fallback is allowed only for explicit
  backend reasons and must remain measured rather than becoming a second execution engine.
- Native overrides are keyed by complete runtime image identity and guest address. Their original
  calls re-enter retail code through the executor; do not retain generated `super` bodies.
- Frame suspension, host work, interrupts, exceptions, and title exit use explicit bounded executor
  exits. C++ unwinding through JIT frames is forbidden.
- The static translator, generated corpus, dispatch adapters, seed inputs, and static-only tests are
  deleted. Do not reintroduce them.

## Current title discipline

Crash Bandicoot (`SCUS_949.00`) is the active title. Its product runs a disc-backed level, presents
frames, and takes input through the authenticated BIOS `PadRead` word at `0x80057054`; the level's
camera does not yet publish a centre (issue 0023), and representative gameplay is the gate (issue
0013). Framework `Pad` owns device polling and the finalized active-low PSX mask;
`titles/crash1/input/crash1_bios_pad_input.*` owns only the authenticated combined word and Crash's
byte order, published before retail `PadUpdate`.

The product starts through psxport's `psx::host::ProductHost` over `crash::CrashCatalog`
(`product/crash_catalog.*`, Crash 1 and 2): zero arguments open the title picker, `pick crash1` or `pick crash2` boots
it, and `session return` comes back. Boot is the host's generic `TitleSession` sequence; Crash 1's
own steps live in `Crash1Runtime` hooks (`discEnvVar`, `registerOverrides`, `bootInit`, `reportRun`).

Do not begin Crash 2 or Crash 3 execution migration until Crash 1 reaches representative gameplay
through Lightrec with its native owners active. Their verified identities, addresses, VSync bodies,
and independent CPU boundaries remain valid evidence, not permission to reuse Crash 1 behavior.

All picture work remains RE-driven. Native producers consume pre-GTE game state. Widescreen changes
owned camera/projection state deterministically; interpolation consumes authoritative
previous/current simulation transforms at presentation time. GTE/OT/GP0 output and framebuffer
pixels are diagnostics, never native producer input.

## How the tree is verified

`uv run --frozen python tools/verify.py` is the gate: it configures, builds, and runs CTest, the
framework C++ policy check. Unit tests live in `tests/`, and the launcher
and provisioning owners have their own tests there. The `probe_crash1_*` tools in `tools/` drive the
product through the framework's loopback control channel and are maintainer legs, not gates; they
need the user's disc and are run by hand.

Never commit game media, extracted executables, generated guest code, `.env`, traces, or
machine-specific paths. Runtime diagnostics use `scratch/`; compiler output uses top-level `build/`.
