# Project state

Factual capability coverage. Epic intent lives in `docs/project-goals.md`, ownership in
`docs/codemap.md`, migration order in `docs/migration.md`, the ordered RE evidence chain in
`docs/re-frontier.md`, and atomic work in `docs/issues/`.

| ID | Capability | State | Evidence or gap |
|---|---|---|---|
| S001 | Serial-identified USA executable facts for Crash 1, 2, 3 | verified | `tools/verify_executable.py` matches size, SHA-1/SHA-256, PS-X EXE header, region markers and VSync body for `SCUS_949.00`, `SCUS_941.54`, `SCUS_942.44` |
| S002 | Disc provisioning selects and verifies one boot executable | verified | `tools/provision_title.py` follows `SYSTEM.CNF`; `tests/test_provision_titles.py` covers three-title selection, precedence, ambiguity and Crash 3's `DRAGON/SPYRO.EXE` decoy |
| S003 | Independent CPU comparison of the resident boot spine | partial | 34/34 register/memory agreement through the first post-syscall B0 dispatch per title; Crash 1's ordered oracle still stops at local wrapper `0x8004323C`, before the non-link A(44h) tail dispatch |
| S004 | Preserved Crash 1 compatibility frontier | partial | frozen evidence from the retired static route (1,172/1,172 field fences, 3D menu); do not rebuild or rerun it |
| S005 | Game-owned native renderer submission | missing | no pre-GTE producer, render queue or ordering owner exists; the guest submits its own GP0 traffic, which does not satisfy this |
| S006 | Widescreen through owned camera/projection state | partial | `crash1_widescreen` + `game/render/widescreen/guest_projection_publication` move OFX and hold OFY/H, and a live unpaused N. Sanity Beach level is now presented on the widened 684-wide canvas from the corrected `$r31 - 8` call-site recovery (203 centre publications in a 200-frame run, `last_retail_centre=(0,0)`); what is still missing is extra FIELD OF VIEW — `OFX + margin` translates this title's projection rather than widening it, so the added columns show the guest's own fill and not more world (issue 0023) |
| S007 | 60 fps interpolation through owned simulation/transform state | missing | requires S005; no producers, so the Record path presents the guest picture twice per logic frame (S017) |
| S008 | Crash 2 and Crash 3 native/Lightrec products | partial | Crash 2 is a `crash::CrashRuntime` title: SCUS_941.54 boots through Lightrec on the shared libcd, callback, GPU-watchdog and frame-driver owners to the title and the warp-room hub with 0 guest faults, and `warp <id>` (`game/frame/scene_warp.*`) drives its scene request word. No gameplay level is reached yet: every valid id tried (0xC to 0x28) loads a warp-room variant, and scene 0x14 and ids 4 or 0x2A fault or hang (issue 0027). Crash 3 still refuses to boot: its CoreLoop has no stub call after GpuUpdate, so its frame boundary needs a different design (issue 0028) |
| S009 | Playable trilogy | missing | no title reaches representative gameplay; Crash 2 reaches the hub only |
| S010 | Host-owned frame-loop contract with typed guest-VSync exits | partial | three measured VSync leaves bound to typed exits; Crash 1 and Crash 2 have measured advancing loops on the shared `CrashFrameDriver`; Crash 3 has none |
| S011 | Crash 1 native/Lightrec product | partial | Crash 1 now runs on the shared Crash owners (`game/boot`, `game/disc`, `game/frame`) and its 200-frame disc run and recordcheck are unchanged. a disc-backed 200-frame run exits 0 with 1,853,304 executed blocks, 20,932,382 instructions, 0 fallback blocks, 203 centre publications through `SetGeomOffset` (`0x80042F8C`, whose per-frame caller `FUN_80017A14` CoreLoop enters unconditionally at `0x800123BC`), `published_H=288` and `last_retail_centre=(0,0)` — the level's centre IS a camera-SHAKE word, so the retail centre is zero in normal play and the owner only logs a non-zero one; a 400-frame input leg moves the BIOS pad word 0xFFFFFFFF to 0xFFFFF7FF while the presented frame content changes, and frame-driven Start reaches a live unpaused N. Sanity Beach (1,384 polygons, `paused 0`) whose presented frame was inspected on the widened 684x240 canvas, so this is a reached level and still not a gameplay claim. The shared owner's call-site recovery was `$r31 - 4`, which names a `jal`'s DELAY SLOT rather than the call (`$r31` is `jal + 8`), so every measured reach read back four bytes past its own site and the owner refused; it is `$r31 - 8` now, and Crash 1 is back on the two measured `jal` sites it always had (issues 0023, 0025). The framework's guest-call census is still collected but never logged (issue 0026) |
| S012 | Linux x86-64 host qualification | partial | the locked asset-free gate and hosted CI pass; real-game execution and release performance are unqualified |
| S013 | Windows x86-64 host qualification | missing | psxport refuses the Windows product target |
| S014 | Apple Silicon macOS host qualification | missing | psxport refuses AArch64; executable memory, ABI, packaging and gameplay unqualified |
| S015 | Android arm64-v8a host qualification | missing | no package, shared Android-port integration, touch layer or AArch64 gameplay run |
| S016 | Loading removal for all three titles | missing | no load operation enumerated or classified yet |
| S017 | Crash 1 on psxport's Record path at 4:3 | partial | `Crash1Runtime::renderCapabilities` ships `RenderPath::Record` with interpolation on and no producers. Headless 1x 4:3 (`aspect=0`, `ires=1`, `PSXPORT_DEBUG=recordcheck`) through the Universal logo fade, title, menu and the N. Sanity Beach map: fps60 off 3,005 present lines, 0 mismatched; fps60 on 3,003 compared presents plus 3,001 composed in-betweens, 0 mismatched, 2,507 real and 2,504 in-between presents at frame 2,501 (the presents double). Scene cuts come from the guest scene words (`CRASH1-SCENE-01`). Gap: no level past the map is reachable (the cross press aborts at `0x80017328`, issue 0013), so gameplay and in-level respawn are unmeasured |

## Comparison baseline

Vanilla PlayStation emulation of each North American retail disc.

| Delta | State | Evidence or gap |
|---|---|---|
| Widescreen | partial | the guest's own centre is published every frame through `SetGeomOffset` and the level is presented on the widened canvas; the widening translates rather than widening the field of view, so no extra world is revealed yet (S006) |
| 60 fps interpolation | missing | S007 |
| Loading removal | missing | S016 |
| Setup | partial | `run.sh` provisions, identity-checks and launches from a fresh clone into psxport's in-window title picker (`Crash1Catalog`, Crash 1 only until issues 0017/0018 close); headless zero-argument run: `picker` lists `crash1`, `pick crash1` boots it, `session return` comes back, and recordcheck at 1x 4:3 shows 2,600 present lines, 0 mismatched, through the logo fade and the intro; no packaged first-run disc picker yet |
| Platforms | partial | Linux x86-64 only; S013–S015 |

Current focus: S008 — find a Crash 2 gameplay scene id (issue 0027) and give Crash 3 its frame boundary (issue 0028). Then S011 — the camera publishes its centre every frame and the level is reached; the next step
is representative interactive gameplay under the player's own control, then the extra field of view for
S006, which needs the title's pre-GTE horizontal frustum rather than the screen offset.
