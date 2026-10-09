# RE frontier

Ordered reverse-engineering dependencies for the Crash trilogy. A title advances independently until
cross-title measurements prove that an implementation belongs in shared `game/`; directory shape is
not evidence of shared ownership.

The execution-method migration is ordered in `docs/migration.md`. Existing static-path observations
below are frozen evidence for the frontier that Lightrec must reproduce; they are not instructions to
regenerate, build, or run that path, and they do not make it a product fallback.

Statuses: ✅ `re-verified` · 🟡 `re-partial` · 🔬 `in-progress` · ⬜ `todo` · ⏸ blocked upstream ·
➖ `skip-by-design` · ⚠️ `hack`.

## Target identity

### CRASH1-01 — select and measure the Crash Bandicoot executable
- status: re-verified
- deps:
- evidence: Real `SCUS_949.00` measured as 290,816 bytes, SHA-1 `b2af088a162e046ad07f532d46b655cfcc42a05c`, SHA-256 `aabf1464f90b2e0b81e712b77aebbdb88f303b16ce830535e2b0cd886ee280f2`. Its PS-X EXE header declares entry `0x8003E018`, text `[0x80010000,0x80056800)` (`0x46800` bytes), gp `0`, and stack `0x801FFFF0`; the entry lies inside the declared text. Embedded strings `Sony Computer Entertainment Inc. for North America area` and `BASCUS-94900` establish the North American `SCUS-94900` target. `python3 tools/verify_executable.py --check --exe <SCUS_949.00>` matches 11/11 tracked facts, including the VSync body fingerprint.
- where: `titles/crash1/executable.json`, `tools/verify_executable.py`
- gap: This step proves executable identity and header only; CRASH1-02 separately binds it to the supplied disc's boot target. No whole-disc authentication, runtime execution, or gameplay is claimed.
- notes: The executable remains outside git. The manifest contains only derived metadata and embedded text evidence.

### CRASH2-01 — select and measure the Crash Bandicoot 2 executable
- status: re-verified
- deps:
- evidence: The real USA disc's `SYSTEM.CNF` selects `SCUS_941.54`. The extracted 327,680-byte PS-X EXE has SHA-1 `aabdade44a4ca863f71224daef51c45f34fb1bf8`, SHA-256 `6e5b2449310b1ed87915f53c38f296f2152bfd0b42eaca5ef86f74a3d07b6043`, entry `0x80049B2C`, text `[0x80010000,0x8005F800)`, gp `0`, and stack `0x801FFFF0`. Embedded strings identify the North American area, `BASCUS-94154`, and `Crash Bandicoot 2`. `python3 tools/verify_executable.py --title crash2 --check --exe scratch/bin/crash2/SCUS_941.54` matches 11/11, including the VSync body fingerprint.
- where: `titles/crash2/executable.json`, `tools/verify_executable.py`
- gap: This proves executable identity and header only; CRASH2-02 separately binds it to the supplied disc and validates the first executed boundary. It does not authenticate the whole disc or prove boot.
- notes: The measured `SCUS-94154` title codeword and every address are independent of Crash 1's `SCUS-94900` integration.

### CRASH2-02 — provision the selected disc and verify the first crt0 call
- status: re-verified
- deps: CRASH2-01
- evidence: The shared serial-aware provisioning path publishes only after SYSTEM.CNF boots SCUS_941.54 and the executable matches 11/11 facts. Its 10-case both-answer suite includes three-way title environment isolation and wrong-title refusal. Against clean pinned psxport bc8c8897, cached real SCUS_941.54 passes the executable fact check, oracle_spike 43/43, and crt0_extract/independent CPU agreement 6/6 at executed jal ordinal 1, step 81,725, target 0x8001144C: gp 0x8005F17C, a0 0x8006F1F4, sp 0x801FFFF8, and heap size 0x0018FE08.
- where: `tools/provision_title.py`, `tests/test_provision_titles.py`, CMake `crash2_oracle_boot_check`, gitignored `scratch/bin/crash2/SCUS_941.54`
- gap: This proves only the first real crt0 call boundary. No later Crash 2 execution, BIOS continuation, hardware, native boot, or gameplay is claimed.
- notes: The 6/6 real-byte cross-check was reproduced at psxport bc8c8897. No disc was configured for a fresh extraction in that run; prior SYSTEM.CNF provisioning evidence remains the disc-selection authority.

### CRASH2-03 — own measured executable facts through the derived runtime
- status: re-verified
- deps: CRASH2-02
- evidence: Against clean pinned psxport bc8c8897, Crash2Runtime directly inherits title-agnostic BoundaryRuntime, which inherits GameRuntime; legacy views and unmeasured runtime products remain null, and the title explicitly reports `guestVramIsPicture=false` because no frame producer exists. GuestProgramImage matches 15/15 fields from real SCUS_941.54 and shipping crt0_extract, including game main 0x80049BD4 independently observed at oracle call three; an altered global pointer produces one named disagreement, authoritative Clang CTest passes 5/5, and psxport_sync checks the same scratch/build-clang provenance.
- where: `game/boot/boundary_runtime.*`, `titles/crash2/entry/crash2_runtime.*`, `tools/verify_runtime_image.py`, `tests/crash2_runtime*.cpp`
- gap: The typed runtime owns measured executable facts and game main `0x80049BD4`; CRASH2-04 preserves the later boundary evidence, and CRASH2-JIT-01 owns future product execution.
- notes: BoundaryRuntime shares only refusal/no-invented-products integration invariants. The game-main address is owned by Crash 2's manifest and runtime because the independent CPU reaches it at call three.

### CRASH2-04 — recorded resident boundary evidence
- status: re-partial
- deps: CRASH2-03
- evidence: Real SCUS_941.54 emits 998 candidates from 270 static seeds with zero overlays under clean psxport `8611d756` (emitter 2026-08-24.2). Generated execution and the independent Mednafen CPU agree 34/34 at each of eight calls: 0x8001144C, 0x800117BC, game main 0x80049BD4, 0x80015614, 0x8004B1B8, 0x8004EC30, 0x8004F1F8, and 0x80049D1C. Call eight is the measured addiu-a0-1/syscall-0 wrapper. At step 85898 the oracle records Cause `0x20` and EPC `0x80049D20`; the shipping HLE retains that record, returns prior IRQ 1, disables delivery, and both CPUs pop the Status mode stack once to resume at `0x80049D24`. Their first post-return state then agrees 34/34 at dynamic BIOS dispatch PC `0xB0`. The 16/16 suite includes altered-frontier, out-of-text seed, 7/8 short-window, repeated-target, altered-register, wrong-syscall-target, and wrong-selector refusals.
- where: frozen evidence only; the static translator, seeds, and boundary tests were deleted by the execution migration and are not restored. The typed runtime facts it bound live in `titles/crash2/executable.json` and `titles/crash2/entry/crash2_runtime.*`.
- gap: Equality is proven from entry through the first post-syscall B0 dispatch boundary. The B0 HLE call itself, later BIOS/hardware boot, frame output, and the remaining 990 emitted bodies are unverified.
- notes: Static discovery counts are not execution provenance; only nine addresses are execution-proven. No Crash 1/3 seed or address was reused. Preserve this measurement without extending the static path; Crash 2 later reproduces it through Lightrec after Crash 1's gameplay gate.

### CRASH3-01 — select and measure the Crash Bandicoot 3 executable
- status: re-verified
- deps:
- evidence: The real USA disc's `SYSTEM.CNF` selects `SCUS_942.44`, not its bundled `DRAGON/SPYRO.EXE`. The extracted 333,824-byte PS-X EXE has SHA-1 `ffa9252384eb16ce5ae14061bb87c86701ab4893`, SHA-256 `1b93cc563c90966e1d79b750f7d19bc1662859cceeb991775e9e9a22656d71d1`, entry `0x800489F8`, text `[0x80010000,0x80061000)`, gp `0`, and stack `0x801FFFF0`; the North America marker is present. The executable gate matches 11/11 tracked facts. The separately extracted 393,216-byte `DRAGON/SPYRO.EXE` disagrees on name, size, both hashes, entry, and text size.
- where: `titles/crash3/executable.json`, `tools/verify_executable.py`, gitignored `scratch/bin/crash3/SCUS_942.44`
- gap: This proves executable identity and the disc's configured boot selection only; it does not authenticate the whole disc, execute the game, or make bundled demo content part of Crash 3.
- notes: The title codeword is `SCUS-94244`. No Crash 1, Crash 2, or Spyro address is inherited.

### CRASH3-02 — provision the selected disc and verify the first crt0 call
- status: re-verified
- deps: CRASH3-01
- evidence: The shared serial-aware provisioner publishes only after SYSTEM.CNF boots SCUS_942.44 and the executable matches 11/11 facts. Its 10-case suite includes three-way environment isolation, wrong-Crash-title refusal, a positive Crash 3 fixture carrying DRAGON/SPYRO.EXE, and refusal when that bundled path is made the boot target. Against clean pinned psxport 57a17a14, crt0_extract and the independent Mednafen CPU oracle agree 6/6 at executed jal ordinal 1, step 71,790, target 0x800112B8: gp 0x80060878, a0 0x8006EA7C, sp 0x801FFFF8, and heap size 0x00190580. The oracle's 39-case fixture passes independently.
- where: `tools/provision_title.py`, `tests/test_provision_titles.py`, CMake `crash3_oracle_boot_check`, gitignored `scratch/bin/crash3/SCUS_942.44`
- gap: This proves executable selection and the first real crt0 call boundary. Later boundary evidence is separately tracked by CRASH3-04; this step alone proves no BIOS continuation, hardware, native boot, or gameplay.
- notes: The first structural call target is not an A(39h) thunk.

### CRASH3-03 — own measured executable facts through the derived runtime
- status: re-verified
- deps: CRASH3-02
- evidence: Against clean pinned psxport 57a17a14, Crash3Runtime directly inherits the title-agnostic BoundaryRuntime, which in turn inherits GameRuntime; both legacy views and unmeasured runtime products remain null. Its GuestProgramImage matches 15/15 fields derived from real SCUS_942.44 and the shipping crt0 decoder, including gameMainEntry 0x80048AA0 verified as oracle call three; an altered global pointer produces one named disagreement. Authoritative Clang CTest passes 5/5, including all three runtime inheritance tests, the 10-case provisioning contract, and format/size/clang-tidy policy.
- where: `game/boot/boundary_runtime.*`, `titles/crash3/entry/crash3_runtime.*`, `tools/verify_runtime_image.py`, `tests/crash3_runtime*.cpp`, `tests/title_runtime_facts.h`
- gap: The typed runtime owns measured executable facts and game main but still refuses native boot; CRASH3-04 preserves the later boundary evidence, and CRASH3-JIT-01 owns future product execution.
- notes: BoundaryRuntime shares only the refusal/no-invented-products invariant. It contains no inferred Crash engine behavior and does not satisfy SHARED-01.

### CRASH3-04 — recorded resident boundary evidence
- status: re-partial
- deps: CRASH3-03
- evidence: Real SCUS_942.44 emits 986 candidates from 297 static seeds with zero overlays under clean psxport `8611d756` (emitter 2026-08-24.2). Generated execution and the independent Mednafen CPU agree 34/34 at each of eight calls: 0x800112B8, 0x80011628, game main 0x80048AA0, 0x800154AC, 0x8004BFBC, 0x8004F37C, 0x8004F914, and 0x80048C38. Call eight is the measured addiu-a0-1/syscall-0 wrapper. At step 75963 the oracle records Cause `0x20` and EPC `0x80048C3C`; the shipping HLE retains that record, returns prior IRQ 1, disables delivery, and both CPUs pop the Status mode stack once to resume at `0x80048C40`. Their first post-return state then agrees 34/34 at dynamic BIOS dispatch PC `0xB0`. The 16/16 suite includes altered-frontier, out-of-text seed, 7/8 short-window, repeated-target, altered-register, wrong-syscall-target, and wrong-selector refusals.
- where: frozen evidence only; the static translator, seeds, and boundary tests were deleted by the execution migration and are not restored. The typed runtime facts it bound live in `titles/crash3/executable.json` and `titles/crash3/entry/crash3_runtime.*`.
- gap: Equality is proven from entry through the first post-syscall B0 dispatch boundary. The B0 HLE call itself, later BIOS/hardware boot, frame output, and the remaining 978 emitted bodies are unverified.
- notes: Static discovery counts are not execution provenance; only nine addresses are execution-proven. Preserve this measurement without extending the static path. The crt0 decoder's structural libcInit slot 0x800112B8 is not an A(39h) thunk in SCUS_942.44; do not apply the A0 modeled-return path to it.


## Provisioning and oracle

### CRASH1-02 — provision the selected disc without tracking copyrighted data
- status: re-verified
- deps: CRASH1-01
- evidence: `python3 tools/provision_title.py --title crash1 <real USA CHD>` extracted `SYSTEM.CNF` and its boot target through psxport `discdump`; `SYSTEM.CNF` selects `SCUS_949.00`, and the extracted 290,816-byte executable matched 11/11 manifest facts including SHA-256 `aabf1464f90b2e0b81e712b77aebbdb88f303b16ce830535e2b0cd886ee280f2`. The real Crash 2 USA disc refused because it boots `SCUS_941.54`. Nine production-seam tests cover every resolution source, precedence, invalid configured-path refusal, ambiguous drop-ins, successful publish, and identity disagreement without publish.
- where: `titles/crash1/executable.json`, `tools/provision_title.py`, `tools/verify_executable.py`, `tests/test_provision_titles.py`
- gap: Provisioning proves which executable the supplied disc boots and that executable's identity; it does not authenticate every disc sector or prove the game boots.
- notes: Resolution is CLI argument, per-title/generic environment, per-title/generic `.env`, then exactly one repository-root CHD. Invalid configured paths and ambiguous drop-ins refuse rather than selecting another image. Extracted data remains gitignored under `scratch/bin/crash1/`.

### CRASH1-03 — deterministic psxport/oracle boot harness
- status: re-verified
- deps: CRASH1-02
- evidence: Against pinned psxport `7f5d3f13`, the independent CPU fixture passed 39/39 positive, hardware-stop negative, instruction-stepping, RAM-mirroring, and modeled-return checks. `oracle_trace` independently captured the first real crt0 call at step 57,910, target `0x80011A18`; `crossvalidate_crt0.py` compared 6 fields and agreed 6/6 on gp, call target, a0, planned sp, planned a0, and planned a1. The bounded crossvalidator selftest passed 5/5, including unreached-call refusal and wrong-target disagreement; the canonical tracer CLI selftest passed 8/8, including insufficient-window refusal. CMake `crash1_oracle_boot_check` reproduces provisioning, the fixture, and the real comparison from one asset-gated target when a disc is configured.
- where: CMake `crash1_oracle_boot_check`; framework `oracle_spike`, `oracle_trace`, `crt0_extract`, and `crossvalidate_crt0.py`; gitignored boundary trace
- gap: This proves the first independent crt0 window only. BIOS, GPU, SPU, CD, DMA, timers, native product execution, and gameplay remain outside this window.
- notes: The call capture is independent of the symbolic expected target: it records the first executed jal after the entry prologue. Do not call this a full oracle boot or a PC port boot.


## Preserved execution and ownership evidence

### CRASH1-04 — record the resident execution boundary
- status: re-partial
- deps: CRASH1-03
- evidence: Against clean psxport `54af32cb`, the verified USA executable and deliberately empty title seed additions produce 115 static candidate seeds and 666 emitted candidates with zero configured overlays under emitter version `2026-08-26.14`. Generated execution with the direct Crash 1 runtime installed starts at entry `0x8003E018` and agrees 34/34 with the independent Mednafen CPU at eight calls through first syscall `0x8003E1F8`. At step 62,083 the independent CPU records Cause `0x20` and EPC `0x8003E1FC`; the controlled shipping path executes the exact generated wrapper through `rec_syscall`, retains the same Cause/EPC, returns prior IRQ 1, and disables delivery. Both CPUs pop the Status mode stack once and resume at `0x8003E200`; the next dynamic-dispatch state agrees 34/34 at PC `0xB0`, with `t1=0x56` and `ra=0x800431B8`, identifying BIOS B(56h). The complete gate passes 18/18, including manifest-altered syscall and BIOS facts, short 7/8 trace, out-of-text seed, repeated target, wrong syscall target, wrong selector, and altered-register controls. Ghidra 12.0.4 disassembly of the same verified bytes at `0x8004319C..0x80043248` proves the continuation reads C0 slot `+0x18`, copies fourteen words over `[0x80043204,0x8004323C)`, and tail-dispatches A(44h) with `ra=0x800431E8`. PSX-SPX's retail-BIOS analysis identifies slot 6 as C(06h) ExceptionHandler at `0x00000C80`. Against clean pushed psxport `99a42aa3`, the shipping-HLE consumer CTest passes that slot contract. A real ordered independent trace then applies selector-1 return, B(56h) returning `0x8000F800`, and seed `0x8000F818=0x00000C80`; at its next boundary `v0=0x00000CB8`, proving the fourteen-word destination loop completed. `crash1_boot_frontier.*` composes the product to execute B(56h) through shipping HLE and stop at the A(44h) pre-HLE boundary.
- where: `titles/crash1/entry/crash1_runtime.*`, `titles/crash1/debug/crash1_boot_frontier.*`, `titles/crash1/main.cpp`, `titles/crash1/main.cpp`, `titles/crash1/executable.json`, `titles/crash1/bios_contract.json`, `tests/crash1_boot_frontier.cpp`, `tests/crash1_c0_exception_contract.cpp`, CMake target `crash1_port`; the static translator, seeds, and recomp boundary tests were deleted by the execution migration
- gap: Equality is proven from the executable entry through the first post-syscall B(56h) dispatch boundary. Product composition extends to A(44h), but no serialized run has yet proven it reaches that boundary. The ordered real oracle now preserves CPU/RAM through the syscall, B(56h), C0 seed, and fourteen-word copy, but its first-post-model-call policy captures intermediate local wrapper `0x8004323C` with `ra=0x800431E8`. That wrapper tail-dispatches A(44h) via non-link `jr`, and modeled returns cannot currently combine with exact target capture. The A(44h) register file and fourteen destination words therefore lack an independent comparison. Of 666 emitted candidates, eight bodies execute and one further candidate is observed as a call target without executing its body; 657 candidates lack execution provenance. Static table discovery also emits data-like fragments, so neither the 115-seed nor 666-candidate count is a correctness denominator.
- notes: C0 slot 6 is fixed in the shared framework, not overridden by Crash. The unresolved A(44h) oracle boundary remains valid evidence debt. Do not extend or rerun the static path; reproduce the recorded state through Lightrec and use the independent oracle for new comparison evidence. This is neither a rendered frame nor a playable-game claim.

### CRASH1-VSYNC — identify and bound guest VSync
- status: re-partial
- deps: CRASH1-01
- evidence: Verified SCUS_949.00 contains canonical libetc VSync at `[0x8003E4F0,0x8003E638)`. The tracked body SHA-256 is `915c96a114984906cdf7e9db7a82f361d9a716a593f5f8afead14b9f45edb8c7`. The executable gate retains altered entry/body controls. The exact-pinned asset-free runtime test initializes the production platform owner, proves the two GpuUpdate JAL sites remain guest instructions rather than native-function overrides, and verifies that the VSync host service returns a typed `FrameBoundary` while preserving its guest continuation.
- where: `titles/crash1/executable.json`, `titles/crash1/frame/crash1_frame_driver.*`, `Crash1Runtime::platformHlePlan`
- gap: A real Lightrec run has not yet reached either GpuUpdate VSync and resumed at its measured continuation.
- notes: The host advances its own clock at each typed boundary; it never executes the retail busy-wait body.

### CRASH2-VSYNC — identify and bound guest VSync
- status: re-partial
- deps: CRASH2-01
- evidence: Verified SCUS_941.54 contains canonical libetc VSync at `[0x8004A484,0x8004A5CC)`. The tracked body SHA-256 is `25097695f5ebd26728662b7989487b139e140ccf2129a14df10b30a003865b4e`. The executable gate retains altered entry/body controls, and the exact-pinned asset-free contract test proves the production platform owner returns a typed `FrameBoundary`.
- where: `titles/crash2/executable.json`, `titles/crash2/frame/crash2_frame_driver.*`, `Crash2Runtime::platformHlePlan`
- gap: The title still lacks a product and runnable frame step (`CRASH2-FRAME`), so real guest execution has not reached this boundary.
- notes: The address is a Crash 2 fact; neither Crash 1 nor Crash 3 addresses are reusable.

### CRASH3-VSYNC — identify and bound guest VSync
- status: re-partial
- deps: CRASH3-01
- evidence: Verified SCUS_942.44 contains canonical libetc VSync at `[0x8004B2A8,0x8004B3F0)`. The tracked body SHA-256 is `9de0bd51589e40ceb1c85119f26f3a0ab8ffd90e8435df340c529444e8a52e80`. The executable gate retains altered entry/body controls, and the exact-pinned asset-free contract test proves the production platform owner returns a typed `FrameBoundary`.
- where: `titles/crash3/executable.json`, `titles/crash3/frame/crash3_frame_driver.*`, `Crash3Runtime::platformHlePlan`
- gap: The title still lacks a product and runnable frame step (`CRASH3-FRAME`), so real guest execution has not reached this boundary.
- notes: The address is a Crash 3 fact; neither Crash 1 nor Crash 2 addresses are reusable.

### CRASH1-FRAME — recover one complete native-owned frame step
- status: re-verified
- deps: CRASH1-04, CRASH1-VSYNC
- evidence: Verified SCUS_949.00 identifies CoreLoop `[0x80011FC4,0x80012560)`, its next-iteration entry `0x80012030`, GpuUpdate `[0x80016E5C,0x800172E8)`, two display waits at `0x800170F4` and `0x80017124`, and its CNT2 increment event `0x80034504`. `Crash1FrameDriver` enters only through the host shell, services pad/audio and the measured counter clock, resumes the two exact post-wait continuations, and commits one presentation fence. Real headless product launches reached three earlier boot-time guest-VSync callers and made each fatal rather than answering it. Ghidra identifies the first two as libcd initialization `[0x80044E8C,0x8004519C)` and callback/event/pad initialization `[0x8003CB9C,0x8003CD3C)`; title-local runtime owners retain both generated bodies, preserve the state-producing initialization, and issue no guest VSync. The third caller `[0x80042864,0x80042A04)` is the GPU command-queue watchdog: its native owner reads the host-owned compatibility field counter directly and retains both the poll deadline and timeout recovery. Focused Clang tests exercise all three production transitions; their touched sources pass clang-tidy and `crash1_port` rebuilds with them.
- evidence: Ghidra identifies the next startup chain as disc-index reader `[0x8002F8C4,0x8002FD30)` calling stock-libcd CdControl `[0x80043520,0x80043668)`, CdControlF `[0x80043668,0x80043984)`, CdRead `[0x800439A4,0x80043A24)`, CdReadSync `[0x80043984,0x800439A4)`, and the CdSync wrapper/body at `0x80043498`/`0x800440EC`. A serialized real-disc run proved reads stayed sequential after the NSD because only CdControl, not the page loader's CdControlF, updated the native head. Extracted NSD entry 6 requires LBA 51468 while guest RAM came from LBA 51376/NSF offset 34. `crash1_disc_index_io.*` now authenticates and binds CdControlF through a no-result ABI adapter to psxport's existing synchronous command owner, preserving every generated body. An isolated rerun then read LBA 51468 and delivered the exact extracted entry-6 payload, SHA-256 `ec3f010049a1b98cf0561ce1163c45992ca4b10bcdeb5bd792df46aaad22c937`; the retail decompressor advanced through later pages. Static inspection of the next refusal found the manifest's done flag transposed as `0x80036428`; the authenticated transition LUI/LW pair loads `0x80056428`. The corrected manifest records both address and load site, and the executable gate decodes and cross-checks that relationship. Against pinned psxport `3c342ec3`, a fully isolated PSX-render run reconciled 120/120 host frame fences through frame 119, returned normally, and emitted no guest-VSync, fatal, error, or watchdog-timeout line. Presented frames 30, 60, and 119 were visually inspected as the centered SCEA Presents splash. The focused Clang test proves Setloc and stale-result-pointer isolation; clang-tidy is clean and the product links.
- where: `titles/crash1/frame/crash1_frame_driver.*`, `titles/crash1/boot/crash1_cd_boot.*`, `titles/crash1/disc/crash1_disc_index_io.*`, `titles/crash1/boot/crash1_callback_boot.*`, `titles/crash1/boot/crash1_gpu_watchdog.*`, `titles/crash1/entry/crash1_runtime.*`, `titles/crash1/main.cpp`
- gap: The native-owned frame-step objective is verified for 120 consecutive intro frames. Gameplay progression remains unverified, and native graphics producers and interpolation remain separate ungrounded frontiers; the widescreen projection owner is built but no widened frame is proven (issue 0023).
- notes: The host shell invokes only this driver. The boot owners are not fake VSync answers: the libcd owner keeps the host controller and reproduces measured library state, the callback owner preserves 21 non-delay calls and removes four hardware-settle delays owned by the host, and the GPU watchdog reads `Timing::vblank` without dispatching the guest leaf. The stable-source page candidate is not in the product; CdControlF fixes the upstream seek instead.

### CRASH1-POOL — recover the size-class block pool that the product faults in
- status: re-verified
- deps: CRASH1-04
- evidence: The product's stop address was a FAULT, not a budget exit: the product's own log said
  `frame 0 left guest execution at 0x800159A8 with fault after 8177050 cycles (Lightrec execution
  fault)`, and `runtime/cpu/lightrec_executor.cpp:624` is the only site that returns that detail.
  `0x800159A8` is the word `0x8C620004` = `lw $v0,0x4($v1)`, the second class-field read of the
  size-class cell lookup whose entry is `0x80015978`; the guest printed `invalid load/store at address
  PC 0x00800004` immediately before, so `$v1` was `0x00800000`, which is unmapped. The image settles
  the class rule: `beq $2,$4` at `0x8001599C` and `bne $2,$4` at `0x800159B0` both test `$a0`, the
  bounded sibling at `0x800159E8` does the same, and the allocate path writes and compares the
  unshifted word at `0x80012FFC` — so the class a cell is compared against is the request word WHOLE,
  not `request >> 13`. The bucket mask is `0x3FC`, the cell stride 8, the class field +4, and the
  bounded sibling `0x800159C4` has **0** call sites. `crash1_block_pool.*` owns the lookup natively.
  Correcting the class rule turned a fault into a complete run: unservable classes 5 -> 0, executed
  blocks 1,689,262 -> 3,461,249, `fallback_blocks` 0 in both arms of the controlled pair.
- where: `titles/crash1/render/crash1_block_pool.*`, `titles/crash1/executable.json` ->
  `runtime.block_pool`, `tests/crash1_block_pool.cpp`, issue 0020
- gap: The allocate path at `0x80012F10` is recovered as a readable model but is deliberately NOT
  installed as an override, because nothing establishes that the guest reaches it: every lookup in
  the disc-backed run came from `ra = 0x80015164` (`FUN_80015118`), never from `0x80013140`. The
  callers' request word is not established either — `0x80015118` passes a tagged handle, not a byte
  count.
- notes: the engine's bound at `0x800159C4` was applied and **measured to break a path retail
  completes** (a disc-backed run faulted at 320,508 cycles with it, and reached the display wait
  without it), so the owner stops at the edge of main RAM instead — a host-memory fact, true for every
  cell retail can read. A controlled pair with the override OFF reaches the same boundary, so the
  owner is transparent. `0x8005C534` is the pool's LOW end, not its high end, and getting that
  backwards rejects the walk on its first step; the test pins cells walked as well as the answer
  because a comment could not have caught it. `[producers] run-end: OtAttr spans recorded 0` is a dead
  counter for this title — its window is the legacy `GameConfig` packet pool, which a typed
  `GameRuntime` never fills — so the guest's real submission is 239,549 GP0 primitives over 400
  frames, and 6 of 6 captured presented frames carry a picture.

### CRASH2-FRAME — recover one complete native-owned frame step
- status: todo
- deps: CRASH2-04, CRASH2-VSYNC
- evidence: The title runtime owns a refusing driver and no shipping product.
- where: `titles/crash2/frame/crash2_frame_driver.*`
- gap: Continue the title-specific boot spine before assigning any frame semantics.
- notes: Do not borrow Crash 1's loop boundary merely because the titles share an engine lineage.

### CRASH3-FRAME — recover one complete native-owned frame step
- status: todo
- deps: CRASH3-04, CRASH3-VSYNC
- evidence: The title runtime owns a refusing driver and no shipping product.
- where: `titles/crash3/frame/crash3_frame_driver.*`
- gap: Continue the title-specific boot spine before assigning any frame semantics.
- notes: Do not borrow Crash 1 or Crash 2 loop boundaries.

### SHARED-01 — prove cross-title shared-engine ownership
- status: todo
- deps: CRASH1-04, CRASH2-01, CRASH3-01
- evidence:
- where: `game/`
- gap: No cross-title function/state correspondence has been measured, so no native implementation belongs in shared `game/` yet.
- notes: Similar directory roles or franchise lineage are leads, not ownership proof.

### CRASH1-05 — identify camera state and graphics submitters
- status: in-progress
- deps: CRASH1-FRAME
- evidence: Static retail/Ghidra and address-aligned reference source identify GfxUpdateMatrices 0x80017A14 camera inputs and GoolObjectTransform 0x8001DE78 pre-GTE object submitter. Against exact psxport `fb08d30f`, one isolated real-disc run reconciled 1,172/1,172 frame fences with no dropped layer and visually advanced from SCEA through Universal and Naughty Dog to the 3D Crash title/main menu. Fntrace measured CamUpdate, GfxUpdateMatrices, and GfxLoadWorlds on all 1,172 frames; GoolObjectTransform first ran at frame 324 and totaled 13,156 calls, compressed vertices first ran at frame 324 and totaled 9,253 calls, and static vertices first ran at frame 630 and totaled 543 calls, all with zero ABI violations.
- where: `titles/crash1/`, eventually `game/render/` only where SHARED-01 proves ownership
- gap: The BIOS auto-pad publication is closed — a held button moves the word at `0x80057054`, the captured frame, and the present counter, and the title reaches a real level. The camera's CENTRE PUBLICATION is closed too: `SetGeomOffset` 0x80042F8C is reached by two `jal`s only, the per-frame one at 0x80017F00 inside `GfxUpdateMatrices` 0x80017A14 which CoreLoop enters unconditionally at 0x800123BC, and a 200-frame 16:9 run exits 0 with 203 centre publications and a retail centre of (0, 0) — the level's centre is a camera-SHAKE word (issue 0023). What is open is the horizontal FIELD OF VIEW: `OFX + margin` translates this title's projection rather than widening it, so the widened canvas's added columns carry the guest's own fill and not more world. The extra columns need the pre-GTE horizontal frustum the title's view matrix carries, which is a native producer over camera/projection state. Representative gameplay remains the gate.
- notes: Resolve from game state before GTE submission, never from GTE/OT/GP0 output. `GfxUpdateMatrices` is the camera snapshot point after `CamUpdate`; `GoolObjectTransform` is the object producer seam because it still owns object identity, animation type/frame, asset, and world transform before the Svtx/Cvtx GTE paths. The call-site recovery for the projection owner is `$r31 - 8`: a `jal` links to its own address plus its delay slot plus four, and `$r31 - 4` names the delay slot (issue 0025).

### CRASH1-06 — owned widescreen and interpolation paths
- status: todo
- deps: CRASH1-05
- evidence:
- where: not assigned
- gap: Widescreen HAS an owned projection producer (`game/render/widescreen/guest_projection_publication.*` over the three per-title owners), the guest's centre is published through `SetGeomOffset` on every frame, and a live unpaused N. Sanity Beach level is presented on the widened 684-wide canvas — but the rule TRANSLATES the projection (`OFX + margin`, H held), so the widened canvas's added columns show the guest's own fill and not more world. Revealing them needs the title's own horizontal frustum, which is CRASH1-05's pre-GTE camera/projection state, and the HUD's screen-space overlays need their own anchoring. Interpolation still lacks an authoritative simulation tick plus owned previous/current camera and object-transform producers, and no native renderer submission owner exists to consume either result.
- notes: Enhancements must remain off during faithful/oracle verification. The centre publication is no longer the prerequisite; the pre-GTE horizontal frustum is.

## Runtime execution migration

### CRASH1-JIT-01 — reproduce the current menu and PadRead frontier through Lightrec
- status: re-verified
- deps: CRASH1-02, CRASH1-03, CRASH1-FRAME
- evidence: The authenticated native/Lightrec product maps `SCUS_949.00` through psxport's per-`Core` executor, reaches the 3D menu, and now runs a disc-backed level to completion: 400 frames, 3,461,249 executed blocks, 46,438,388 executed instructions, 0 fallback blocks. A held button moves the BIOS `PadRead` word at `0x80057054` (active-low bit 11), changes the captured frame, and advances the present counter, and Start plus a menu tap walks the UIS copyright screen, the menu, and a real level ("N. Sanity Beach") — so the earlier "host input never reaches the title" reading was a fact about a run that presented nothing.
- where: psxport's target per-`Core` executor; `titles/crash1/entry/crash1_runtime.*`, `titles/crash1/frame/crash1_frame_driver.*`, `titles/crash1/input/crash1_bios_pad_input.*`
- gap: The level's camera publishes no centre through `SetGeomOffset` in a live unpaused level (issue 0023), so the product reaches a rendered level but not interactive gameplay. Keep the linked product boundary free of a full-game/player-selectable interpreter and generated guest code.
- notes: The static translator, corpus, dispatch adapters, seeds, and static-only tests were deleted
  before this integration; do not restore a compatibility path.

### CRASH1-SCENE-01 — the scene words a presented frame cuts on
- status: re-partial
- deps: CRASH1-JIT-01
- evidence: Ghidra over `SCUS_949.00`. `FUN_80011FC4` (CoreLoop) tests the request word `0x80056714` at `0x800121C0..0x80012300`: `-1` is no request; a scene id unloads the scene (`FUN_80016420` clears the loaded flag `0x8005C528`) and loads it through `FUN_80015B58`, which resets the request to `-1` (`0x80015BA0`) and stores the new id to `0x80056710` and `0x8005C52C`. `-2` reloads the scene saved at `0x800579A4` and then restores the checkpoint snapshot at `0x80057974` (`FUN_80026650`; the same call stores `-2` through `gp+0x318` at `0x80026698` when the snapshot belongs to another scene). Death or exit states `0x200/0x300/0x400` in `0x800618D0` request scene `0x19` (`0x80012214`). Script op 9 of the object interpreter `FUN_800201DC` stores `operand >> 8` to the request word (`0x800228A4`). Live: the title, menu and map screens all run in scene `0x19` with the request at `-1` (`rw 80056710`).
- where: `titles/crash1/frame/crash1_frame_cut.*` samples the id and the request once per logic frame; `titles/crash1/executable.json` `runtime.native_frame.scene`
- gap: A checkpoint respawn inside the loaded scene (`FUN_80026650` with a matching snapshot scene) restores objects in place and raises neither word, so it is not a cut yet. The first level past the map aborts at the `0x80017328` frame boundary (issue 0013), so no scene change after the first frame has been observed live; the cut rule is exercised by `tests/crash1_frame_cut.cpp` only.
- notes: A cut is the frame after a pending request, or a changed id; the first frame of a session is a cut.

### CRASH1-JIT-02 — prove representative interactive gameplay
- status: todo
- deps: CRASH1-JIT-01
- evidence:
- where: Crash 1 product plus separately built independent-oracle harness
- gap: Enter and sustain a representative level with the PadRead owner and all native services active; compare timing, interrupts, CPU/memory and relevant device state; exercise an override, original call, and executable-memory invalidation in positive and controlled-negative cases; qualify released hosts.
- notes: This is the fidelity gate for the replacement product. No compatibility mode exists.

### CRASH2-JIT-01 — migrate Crash 2 after Crash 1 is complete
- status: partial (boots to the title and warp-room hub on the shared Crash owners; no gameplay scene id found, issue 0027)
- deps: CRASH1-JIT-02, CRASH2-02, CRASH2-03, CRASH2-VSYNC
- evidence:
- where: `titles/crash2/` plus psxport executor
- gap: Reproduce Crash 2's recorded resident boundary through Lightrec, recover its title-owned frame/services, and pass representative gameplay.
- notes: Do not inherit Crash 1 addresses, seeds, or behavior.

### CRASH3-JIT-01 — migrate Crash 3 after Crash 2 is complete
- status: todo (mapped: CoreLoop 0x8001166C, scene request 0x80060AC0, loop-top 0x800116D8, GpuUpdate FUN_80016634 with VSync calls near 0x80016AA8; stub calls at 0x80011868 and 0x80011D90 only; libcd bodies match Crash 2; issue 0028)
- deps: CRASH2-JIT-01, CRASH3-02, CRASH3-03, CRASH3-VSYNC
- evidence:
- where: `titles/crash3/` plus psxport executor
- gap: Reproduce Crash 3's recorded resident boundary through Lightrec, recover its title-owned frame/services, and pass representative gameplay.
- notes: Preserve `SYSTEM.CNF` selection and reject `DRAGON/SPYRO.EXE` as the Crash 3 product image.
