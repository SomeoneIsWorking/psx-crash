# Prior art

Outside work on the Crash 1/2/3 USA executables; a reference names addresses, our measurement decides behaviour.

Our targets: Crash 1 `SCUS_949.00` SHA-1 `b2af088a162e046ad07f532d46b655cfcc42a05c`; Crash 2 `SCUS_941.54` SHA-1
`aabdade44a4ca863f71224daef51c45f34fb1bf8`; Crash 3 `SCUS_942.44` SHA-1 `ffa9252384eb16ce5ae14061bb87c86701ab4893`.
Address checks ran `decomp_pipeline.py --function-at` on our images (2026-10-13) unless marked "our docs"; licences,
trees and push dates were re-read through the GitHub API on 2026-11-13.

## Decomps (submodules)

| submodule | URL | pinned | licence | target build and match | progress (verified in tree) | what we may take |
|---|---|---|---|---|---|---|
| `external/crash-bandicoot-decomp` | https://github.com/khasinski/crash-bandicoot-decomp | `5a4d31a8` (2026-10-20) | none (`LICENSE.md`: no licence chosen) | Crash 1 `SCUS_949.00`: `configs/USA/checksum.sha` is our SHA-1; `CORE_Main 0x80011D88`, `CORE_Loop 0x80011FC4`, `CORE_VSync 0x8003E4F0`, `GOOL_InterpretObject 0x800201DC` are Ghidra entries in our image | badge `docs/badges/functions.json` and `code.json` claim 100% matching; tree has 400 `src/**/*.c` and no `asm/` files; `docs/names.md` names 825 functions | evidence only: names, struct layouts (`docs/STRUCTURES.md`), GOOL notes; no code until it has a licence |
| `external/crash2-decomp` | https://github.com/Gabriel-TwoBits/crash2-decomp | `d7da30d0` (2026-10-25) | GPL-3.0 | Crash 2 `SCUS_941.54`: `config/SCUS_941.54.sha1` is our SHA-1; splat config `config/SCUS_941.54.yaml` | README: 270 of 466 game functions byte-matching (20.53% of bytes), the rest behaviour-equivalent C tested against retail; 33 `src/**/*.c`, per-function notes in `config/match/notes/` | evidence only (GPL): names, types, function semantics; no code copied into our MIT tree |

## Other projects

| project | URL | kind | licence | target build | build match (addresses checked) | what we may take | notes |
|---|---|---|---|---|---|---|---|
| khasinski/crash-bandicoot-pc | https://github.com/khasinski/crash-bandicoot-pc | native PC port built from the Crash 1 decomp (imported at `965a1bd`) on PsyZ | none (`LICENSE.md`: no licence chosen) | Crash 1 `SCUS-94900` | matched through its source decomp | evidence only | Verified 60 fps: `src/platform/interp.c` snapshots game memory, draws a midpoint frame with the logic held at 30 Hz, and restores memory, the same design as our replay interpolation. Last push 2026-10-20. |
| wurlyfox/c1c | https://github.com/wurlyfox/c1c | MIPS-to-C translation plus hand decomp over a PCSX-R core | none | Crash 1 `SCUS_949.00` (NTSC) | matched: `docs/FUNCTIONS.md` lists `CORE_Main 0x80011D88`, `CORE_Loop 0x80011FC4`, `0x80017A14`, `0x8001DE78`, `CORE_VSync 0x8003E4F0` | evidence only (names) | Archived; last push 2015-07-22. |
| CBHacks wiki, Game Loop | https://wiki.cbhacks.com/w/Game_Loop | RE notes | none stated | Crash 1 (region not stated) | matched (our docs): loop `0x80011FC4`, scene load `0x80015B58`, `UpdateMatrices 0x80017A14`, GPU routine `0x80016E5C`, object interpreter `0x800201DC` | evidence only | Other per-frame callees (`0x800134C8`, `0x80025928`, `0x8002B2BC`, `0x8001D5EC`) are unverified leads for issue 0023. |
| Matteo842/CrashBandicoot-Launcher | https://github.com/Matteo842/CrashBandicoot-Launcher | static recompilation (RecompOne) plus launcher | MIT | Crash 1 `SCUS-94900` | matched (our docs): `0x8003E4F0` VSync, `0x8003E638`, `0x80011FC4` loop | may copy with attribution; little fits a dynarec port | Claimed widescreen in `docs/NATIVE_WIDESCREEN.md`, unverified. Last push 2026-11-12. |
| omegakatana92/CrashBandicootRecomp | https://github.com/omegakatana92/CrashBandicootRecomp | psxrecomp config and seeds | none | Crash 1 `SCUS-94900` | matched: `game.toml` entry `0x8003E018`, text size `0x46800` equal our header | evidence only | Seeds only. Last push 2026-09-03. |
| ignzio/CB2-Online | https://github.com/ignzio/CB2-Online | PC runner (own R3000/GTE/GPU/SPU platform layer running the retail code) with online play | MIT (`LICENSE` text; GitHub reports NOASSERTION) | Crash 2 `SCUS_941.54` and PAL `SCES_009.67` | not checked: no address list read | may copy with attribution | Claimed "60 fps smoothing (unstable)": code exists in `src/interp.cpp`. GOOL VM and NSF/NSD readers in `src/gool_*.cpp`, `src/nsf.cpp`, `src/nsd.cpp`. Last push 2026-11-05. |
| Gabriel-TwoBits/crash2-pc | https://github.com/Gabriel-TwoBits/crash2-pc | native PC port compiling the crash2-decomp C (submodule `decomp/`) | GPL-3.0 | Crash 2 `SCUS-94154` via the decomp | matched through its submodule | evidence only | No 60 fps or widescreen code in the tree. Last push 2026-10-25. |
| ughman/c2c | https://github.com/ughman/c2c | MIPS-to-C translation plus hand decomp | none | Crash 2 PAL `SCES_009.67` | mismatched: `CORE_VSync 0x8004A864` vs our `0x8004A484`; pre-shift code agrees (`CORE_Main 0x800117BC`, `CORE_Loop 0x80011800`) | evidence only (names, after measuring the PAL offset) | Last push 2020-09-30. |
| Zumbo06/Crash2Recomp | https://github.com/Zumbo06/Crash2Recomp | psxrecomp static recompilation | none on GitHub; README states PolyForm Noncommercial 1.0.0 for psxrecomp | Crash 2 `SCUS-94154` | matched: seeds contain our `0x800117BC`, `0x80049BD4`, `0x80015614`, `0x8004B1B8`, `0x8004EC30`, `0x80049D1C` | nothing from the runtime; the seed list is a function-entry census | Last push 2026-10-08. |
| Purplesmaug96/c2-recomp | https://github.com/Purplesmaug96/c2-recomp | psxrecomp static recompilation | MIT | Crash 2 `SCUS-94154` | matched: `game.toml` entry `0x80049B2C` | nothing from the runtime | Single push 2026-08-01. |
| Matteo842/CrashBandicoot2-Launcher | https://github.com/Matteo842/CrashBandicoot2-Launcher | static recompilation (RecompOne) plus launcher | MIT | Crash 2 PAL `SCES_009.67` | mismatched build: PAL | nothing | Not playable per its README. Last push 2026-09-16. |
| NyperYuhgard/Crash-Bandicoot-Warped | https://github.com/NyperYuhgard/Crash-Bandicoot-Warped | Ghidra project plus armips asm patches | none | Crash 3 `SCUS_942.44` | matched: the committed retail exe blob equals ours; `Game_Initialize_And_Boot 0x80011628` calls `0x80048AA0` and `0x800154AC` | evidence only (names from the `.s` files) | Not a decomp: no C or splat config. Distributes the retail executable and `armips.exe`; do not download or open its `.gpr`/`.rep`. Last push 2026-08-04. |
| alexbeavs-ps1-ports/crash-bandicoot-3-warped-recomp | https://github.com/alexbeavs-ps1-ports/crash-bandicoot-3-warped-recomp | psxrecomp static recompilation | PolyForm-Noncommercial-1.0.0 (`LICENSE`) | Crash 3 `SCUS-94244` | matched: `seeds/ghidra_funcs.txt` contains our `0x80048AA0`, `0x800154AC`, `0x80011628`, `0x8004F37C`, `0x8004B2A8` | nothing from the runtime; seeds as a census | Last push 2026-11-13. |
| omegakatana92/CrashWarpedRecomp | https://github.com/omegakatana92/CrashWarpedRecomp | psxrecomp config and seeds | none | Crash 3 USA (README) | not checked | nothing from the runtime | Last push 2026-09-03. |
| Purplesmaug96/c3-recomp | https://github.com/Purplesmaug96/c3-recomp | psxrecomp static recompilation | MIT | Crash 3 (no serial named) | not checked: no address list | nothing from the runtime | Last push 2026-08-02. |
| ManDude/goocdump | https://github.com/ManDude/goocdump | GOOL script decompilation, all three games | none | per-region folders | not applicable (GOOL bytecode) | evidence only | Last push 2021-01-28. |
| cbhacks/CrashEdit | https://github.com/cbhacks/CrashEdit | NSF/NSD level editor | none for project code (GitHub NOASSERTION) | all three, data formats | not applicable | evidence only | Last push 2026-08-18. |

Not found (2026-11-13): a decomp of Crash 3 `SCUS_942.44` (needs our own MIT repo); a licensed Crash 1 decomp; any
Crash 1/2/3 entry on decomp.dev; a widescreen or 60 fps patch for Crash 3. Searched GitHub repositories (titles,
serials, "decomp", "recomp", "port", "widescreen", "60fps"), decomp.dev, and the web. `dguillot-gh/crash2-android`
and `dguillot-gh/crash3-android` are gone (404).
