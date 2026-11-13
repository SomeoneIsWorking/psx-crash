# 0001 — Guest addresses from the decomp symbol map

Workspace rule (re-harness, 2026-11-13): RE knowledge lives in the decomp repository; the port reads guest addresses
from the decomp's symbol map as generated constants, never a hand copy. Each title's decomp is a submodule:

| title | decomp | symbol map |
|---|---|---|
| Crash 1 `SCUS_949.00` | `external/crash-bandicoot-decomp` (our fork; no licence) | `configs/USA/sym.main.txt` (upstream's names, ours appended where it had none) |
| Crash 2 `SCUS_941.54` | `external/crash2-decomp` (our fork; GPL-3.0) | `config/symbol_addrs.txt` (as Crash 1) |
| Crash 3 `SCUS_942.44` | `external/crash3-decomp` (ours, MIT) | `config/symbols/scus_942.44.txt` (all ours) |

Not started. It rewrites every title's facts, which collides with work in flight; run it when the tree is quiet, one
title and file per commit.

## Today

- `titles/<t>/executable.json` holds the addresses CMake turns into `CRASH_*` definitions and
  `tools/verify_executable.py` checks against the executable (body hashes, address loads): 130 in Crash 1, 55 in
  Crash 2, 51 in Crash 3. Its body hashes and measured loads are verification data and stay; only the addresses move.
- `titles/<t>/entry/facts.cpp` designated initializers (`.lastStatus = 0x8005C7DCu`): 34, 34 and 27.
- `titles/crash1/render/block_pool.h`: 21, constants and code sites of the lookup at 0x80015978.
- The port's names for all of these were seeded into each map on 2026-11-13. Where upstream already named an address
  (71 in Crash 1, 35 in Crash 2) its name stays and ours is listed in that decomp's `docs/our-names.md`; that file
  also lists the code sites (return addresses, VSync calls, patched words) by function and offset.

## Generator

The one psxport tool shared with Spyro (psx-spyro plan 0251) and Tomba! 2 (psx-tomba plan 0065):
`tools/symbol_constants.py` reads splat `symbol_addrs` files and writes one header per image under `build/`,
`namespace <title>::sym { inline constexpr std::uint32_t <name> = 0x...u; }`, plus `size` where the map gives one,
through `psxport_symbol_constants(<target> IMAGE <name> MAP <file>...)`. Nothing generated is committed.

- Names are emitted as the map spells them. The port adopts upstream's name where `our-names.md` lists a conflict, and
  the row is deleted when its constant goes; a name we want changed is changed in the fork.
- Crash 2's `func_80011E90` and `D_8005F3EC` are placeholders upstream's C refers to; they get a name in the fork's C
  and map together, in one fork commit.
- Code sites are written as `<function> + 0xNN` against a map symbol.
- `executable.json` keeps a site's measured word and hash; its address becomes a reference to the generated symbol,
  read by CMake and `verify_executable.py` from the same map.

## Order

1. Psxport's tool and CMake function exist once Spyro or Tomba! 2 lands them; build `crash_port` with headers nothing
   includes yet.
2. Crash 2, then Crash 3, then Crash 1: `entry/facts.cpp`, then the `CRASH_*` definitions from `executable.json`;
   one commit per file. Each missing name goes into the decomp first (decomp commit, submodule bump in the port
   change that uses it).
3. Crash 1's `render/block_pool.h`, adopting `Ns_Probe`/`Ns_ProbeSafe` from upstream's map.
4. Build with Clang; `warp` once per title to show the bindings still fire.
