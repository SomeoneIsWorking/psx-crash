# Crash trilogy port

`AGENTS.md` is the repository-local instruction authority. Read it completely before work. The
product architecture and ordered migration are in `docs/migration.md`; factual coverage is in
`docs/project-state.md`; ownership is in `docs/codemap.md`; the ordered reverse-engineering
dependencies are in `docs/re-frontier.md`.

Each Crash product combines native title owners with psxport's pinned Lightrec executor over the
user's verified executable. The gameplay executable has no generated guest corpus, full-game
interpreter, or player-selectable interpreter mode. Lightrec's bounded per-block fallback is allowed
only for explicit backend reasons and remains measured. Do not regenerate, build, or run the static
execution machinery; that machinery has been deleted.

Crash 1's product runs the real disc: it boots, submits its own GP0 traffic, presents frames, and
consumes host input through the authenticated BIOS `PadRead` word at `0x80057054`, which
`crash1_bios_pad_input.*` publishes before retail `PadUpdate` (psxport polls the host pad and
finalizes the active-low mask). The level's camera does not yet publish a centre, and representative
interactive gameplay remains the fidelity gate.

Crash 2 (`SCUS_941.54`) and Crash 3 (`SCUS_942.44`) retain their title-specific executable,
crt0/syscall, game-main, and VSync facts. Crash 3 selection follows `SYSTEM.CNF`, never the unrelated
`DRAGON/SPYRO.EXE` on its disc. Finish Crash 1's representative-gameplay gate before starting either
title's execution migration.

Keep title behavior local until direct cross-title evidence proves a shared `game/` owner. Native
graphics begin at game-state camera and submitter boundaries before GTE projection. Never reconstruct
a product renderer from GTE, ordering-table, GP0, or framebuffer output.
