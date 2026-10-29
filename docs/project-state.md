# Project state

Factual capability coverage. Epic intent lives in `docs/project-goals.md`, ownership in
`docs/codemap.md`, migration order in `docs/migration.md`, the ordered RE evidence chain in
`docs/re-frontier.md`, and atomic work in `docs/issues/`.

| ID | Capability | State | Evidence or gap |
|---|---|---|---|
| S001 | Serial-identified USA executable facts for Crash 1, 2, 3 | verified | `tools/verify_executable.py` matches size, SHA-1/SHA-256, PS-X EXE header, region markers and VSync body for `SCUS_949.00`, `SCUS_941.54`, `SCUS_942.44` |
| S002 | Disc provisioning selects and verifies one boot executable | verified | `tools/provision_title.py` follows `SYSTEM.CNF` |
| S003 | Independent CPU comparison of the resident boot spine | partial | 34/34 register/memory agreement through the first post-syscall B0 dispatch per title; Crash 1's ordered oracle still stops at local wrapper `0x8004323C`, before the non-link A(44h) tail dispatch |
| S004 | Preserved Crash 1 compatibility frontier | partial | frozen evidence from the retired static route (1,172/1,172 field fences, 3D menu); do not rebuild or rerun it |
| S005 | Game-owned native renderer submission | missing | no pre-GTE producer, render queue or ordering owner exists; the guest submits its own GP0 traffic, which does not satisfy this |
| S006 | Widescreen through owned camera/projection state | partial | All three titles present on Record with the canvas widened at 16:9 and the retail centre kept (`crash::Runtime::guestWidescreenProjection`); the OFX owner that translated the picture is deleted. Crash 3 at 16:9 in a level draws geometry into the 684-wide canvas margins, but the world's polygons stop at the 4:3 edge, so the margins are partly drawn (issue 0030: a runtime reject or the camera's precomputed sort list) |
| S007 | 60 fps interpolation through owned simulation/transform state | missing | requires S005; no producers, so the Record path presents the guest picture twice per logic frame (S017, issue 0032) |
| S008 | Crash 2 and Crash 3 native/Lightrec products | partial | Both run as `crash::Runtime` titles in the three-title catalog. Crash 2: boots to the title and warp-room hub with 0 guest faults; `warp 0x15`, `0x10`, `0x25`, `0x1B` run distinct levels; the hub portal and pause menu work headless (issues 0027, 0029); New Game's scene 0x1C is not played through to the hub. Crash 3: boots on Record to the title menu through Crash 1's boundary design at the GpuUpdate return (issue 0028, resolved); `warp 5` and `warp 3` run the N. Gin and Dingodile boss levels, and `warp 0x19` runs into the jet-ski scene since the Lightrec divide fix (issue 0033), but starts it with a zero divisor a console would trap on (issue 0034, open); recordcheck 1x 4:3 3,003 presents, 0 mismatched. Its pad reaches the guest through psxport's SIO path: a down tap at the title menu moves the cursor to LOAD GAME. GpuUpdate's limiter skips both waits on the two frames after a load; those frames commit the fields up to their VBlank (`FrameProgram::limiter`). No title is played from New Game |
| S009 | Playable trilogy | missing | no title reaches representative gameplay; Crash 2 loads its levels through `warp` and through the hub's portal under forced input, and its pause menu works; the New Game path through scene 0x1C to the hub is not walked |
| S010 | Host-owned frame-loop contract with typed guest-VSync exits | partial | `crash::FrameDriver` advances all three titles: typed VSync exits at the measured GpuUpdate sites and a transition override at the loop end; CNT2 runs at the period its `SetRCnt` programs. Crash 1's scene loader swaps buffers twice through `FUN_800172E8` for its loading picture; those waits are `FrameProgram::loadingSwap`, counted apart from GpuUpdate's two |
| S011 | Crash 1 native/Lightrec product | partial | Crash 1 boots on the shared Crash runtime through the logo, intro and title menu; two start taps reach the N. Sanity Beach map; recordcheck 1x 4:3 3,007 presents, 0 mismatched. `warp 3` loads a level Crash stands in; independent oracle comparison of gameplay is open (issue 0013) |
| S012 | Linux x86-64 host qualification | partial | the locked asset-free gate and hosted CI pass; real-game execution and release performance are unqualified |
| S013 | Windows x86-64 host qualification | missing | psxport refuses the Windows product target |
| S014 | Apple Silicon macOS host qualification | missing | psxport refuses AArch64; executable memory, ABI, packaging and gameplay unqualified |
| S015 | Android arm64-v8a host qualification | missing | no package, shared Android-port integration, touch layer or AArch64 gameplay run |
| S016 | Loading removal for all three titles | missing | no load operation enumerated or classified yet |
| S017 | The trilogy on psxport's Record path at 4:3 | partial | All three titles present on `RenderPath::Record` with no producers; Crash 1 and 2 declare interpolation, but each in-between repeats the record before it (issue 0032), and Crash 3 declares none. Headless 1x 4:3 recordcheck, 0 mismatched: Crash 1 3,007 presents to the N. Sanity Beach map; Crash 2 3,012 through `warp 0x15`; Crash 3 3,003 through the title menu and `warp 5`, and 2,508 at 16:9 through `warp 3`. Scene cuts come from the guest scene words (`crash::FrameCut`). Real-window play check (2026-10-16, vsynced, 16:9, fps60 on Crash 1 and 2, 75 s after the warp): Crash 1 `warp 3` missed 1 of 4,510, Crash 2 `warp 0x15` 2 of 4,507, Crash 3 `warp 5` 1 of 2,258, none in a row; 0 reprimes and 0 drops; 0 repeats and 0 steps back. The 5 and 4 presents without an in-between are the session's first, where no earlier record is shown (boot and the warp's load frames). Crash 3 runs without fps60, so its record order is not counted. Gap: recordcheck of a Crash 1 level |

## Comparison baseline

Vanilla PlayStation emulation of each North American retail disc.

| Delta | State | Evidence or gap |
|---|---|---|
| Widescreen | partial | Record widens the canvas at 16:9 for all three titles with the retail centre kept; the guest's horizontal culls are not widened, so the margins are partly drawn (S006, issue 0030) |
| 60 fps interpolation | missing | S007 |
| Loading removal | missing | S016 |
| Setup | partial | `run.sh` provisions, identity-checks and launches into psxport's in-window picker over `crash::Catalog` (Crash 1, 2, 3); headless `pick crash1`, `pick crash2`, `pick crash3` each boot; no packaged first-run disc picker yet |
| Platforms | partial | Linux x86-64 only; S013–S015 |

Current focus: S006 — each title's horizontal culls widened by the margin (issue 0030).
