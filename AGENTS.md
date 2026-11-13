# Crash trilogy port

One native PC product per title: title-owned native subsystems plus psxport's pinned Lightrec
integration executing every remaining retail instruction. Product architecture and the ordered
migration are in `docs/migration.md`. Workspace: `../AGENTS.md`; framework contract:
`external/psxport/AGENTS.md`. Execution policy: the `dynarec-port` and `dynarec-overrides` skills.

## Execution

- The products consume the user's identity-verified executable through the per-`Core`
  psxport-Lightrec executor.
- Frame suspension, host work, interrupts, exceptions, and title exit use explicit bounded executor
  exits. C++ unwinding through JIT frames is forbidden.

## Titles

Crash Bandicoot (`SCUS_949.00`) is the active title. Its product runs a disc-backed level, presents
frames, and takes input through the authenticated BIOS `PadRead` word at `0x80057054`; the level's
camera does not yet publish a centre (issue 0023), and representative gameplay is the gate (issue
0013). Framework `Pad` owns device polling and the finalized active-low PSX mask;
`titles/crash1/input/bios_pad_input.*` owns only the authenticated combined word and Crash's
byte order, published before retail `PadUpdate`.

Crash 2 (`SCUS_941.54`) and Crash 3 (`SCUS_942.44`) retain their title-specific executable,
crt0/syscall, game-main, and VSync facts. Crash 3 selection follows `SYSTEM.CNF`, never the unrelated
`DRAGON/SPYRO.EXE` on its disc.

The product starts through psxport's `psx::host::ProductHost` over `crash::Catalog`
(`product/catalog.*`, Crash 1, 2 and 3): zero arguments open the title picker, `pick crashN` boots
it, and `session return` comes back. Boot is the host's generic `TitleSession` sequence; each title's
steps live in `crash::Runtime` (`game/runtime/runtime.*`) and its `crashN::Runtime final` subclass;
its disc key is in its `TitleIdentity`.

Each title's verified identities, addresses, VSync bodies and CPU boundaries are its own evidence,
not permission to reuse another title's behavior. Keep title behavior local until direct cross-title
evidence proves a shared `game/` owner.

## Decomps

Each title's decomp is a pinned submodule that holds its RE notes and symbol map: Crash 1 `external/crash-bandicoot-decomp`
(our fork of khasinski's; no licence, evidence and names only), Crash 2 `external/crash2-decomp` (our fork of
Gabriel-TwoBits'; GPL-3.0, evidence and names only), Crash 3 `external/crash3-decomp` (ours, MIT). Guest addresses move
from the port's hand-written facts to the decomps' maps by `docs/plans/0001-addresses-from-the-decomp-symbol-map.md`.

## Picture

Native producers consume pre-GTE game state. Interpolation consumes authoritative previous/current
simulation transforms at presentation time. GTE/OT/GP0 output and framebuffer pixels are
diagnostics, never native producer input.

## Commands

There are no unit tests. Build with `CC=clang CXX=clang++ uv run --frozen python
external/psxport/tools/port/consumer_build.py --build build/agent --target crash_port`. The
`probe_crash1_*` tools in `tools/` drive the product through the framework's loopback control
channel; they need the user's disc and are run by hand.
