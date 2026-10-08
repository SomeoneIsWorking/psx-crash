# Project goals

Stable product intent for the Crash engine-lineage repository. Factual coverage belongs in
`docs/project-state.md`; atomic work belongs in `docs/issues/`.

## G001 — playable faithful Crash trilogy ports

Deliver native PC products for the North American releases of Crash Bandicoot, Crash Bandicoot 2,
and Crash Bandicoot 3. Title-owned native subsystems cooperate with psxport's pinned Lightrec
executor, which executes remaining guest code on demand from the user's verified retail executable
and translates it by default.

Why it matters: the repository represents one engine lineage, so a Crash 1-only executable is not the
trilogy product.

Success conditions:

- each title has a serial-verified, asset-safe provisioning path and shipping executable;
- each product reaches sustained gameplay with working presentation, input, audio, saves, and timing;
- `run.sh` launches the current intended product from a fresh clone with documented native dependencies,
  `uv`, and user-supplied game data;
- the gameplay executables contain neither generated guest code, a full-game interpreter, nor a
  player-selectable interpreter mode; link/selector checks prove that separation and measured
  Lightrec fallback remains bounded to explicit backend reasons;
- native overrides and override-bypassing original calls execute through one image-aware runtime
  dispatcher with explicit bounded exits and executable-memory invalidation.
- Linux x86-64, Windows x86-64, Apple Silicon macOS, and Android arm64-v8a are independently
  qualified through their real product boundary; an unavailable host remains explicitly missing
  rather than being represented by a platform-named policy-only job.

Constraints and non-goals:

- no build, install, provisioning, or launch step emits or compiles guest code ahead of time;
- an independent interpreter may be built only as a separate test/diagnostic oracle; the gameplay
  product permits only Lightrec's bounded, measured per-block fallback;
- measurements from the removed static path remain evidence only; no translator, corpus, static
  dispatcher, seed input, compatibility selector, or static-only test returns to the product;
- title addresses and behavior remain title-owned until direct cross-title evidence proves a shared
  engine owner;
- CTR and Crash Bash are separate engines and are not part of this repository's product.

Related state: S001, S002, S003, S004, S008, S009, S011.

## G002 — owned widescreen, interpolation, and native rendering

Provide widescreen, presentation-time interpolation, and game-owned native rendering for all three
Crash titles on top of their faithful execution bases.

Why it matters: these enhancements require semantic access to camera, simulation, transform, and
graphics-submission state; post-projection reconstruction cannot deliver the intended fidelity.

Success conditions:

- each title widens view geometry through an owned camera/projection producer;
- authoritative previous/current simulation and transform state feeds presentation-time interpolation;
- game-owned producers submit native primitives without using GTE/OT/GP0 output as the source of truth;
- enhancement modes remain disabled during oracle comparison and have independent visual/runtime gates.

Constraints and non-goals:

- enhancements never substitute for an unimplemented faithful boot or gameplay stage;
- shared implementation is limited to behavior proven common across Crash 1, 2, and 3;
- native rendering is not required for non-Crash projects merely because this repository owns it.

Related state: S005, S006, S007.

## G003 — Loading removal

Remove storage latency and loading-only waits from every load the game performs, without changing
unrelated scripted timing or faking completion. Loading runs asynchronously and the product goes
straight to the next real presentation.

Success requires each measured load operation to deliver the same payload and terminal state as
retail while omitting its loading-only presentation. Logo screens accept Start/Cross through the
title's recovered cancellation route (or a purpose-built skip establishing the same lifecycle,
resource, and state invariants). Authored transition cutscenes are presentation, not loading, and
remain.

Faster simulation, bypassed lifecycle callbacks, written phase/timer/scene words, and presentation
tricks that hide a wait are not implementations of this goal. Loading removal is suppressed under
oracle comparison.

Contributing state: S016.
