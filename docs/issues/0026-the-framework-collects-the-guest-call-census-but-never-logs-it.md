# 0026 — the framework collects the guest-call census but never logs it

- state: reported to the framework owner; not fixable from this repository
- found: 2026-10-03, while migrating Crash 1 onto psxport `09a5e650`

## What

`psx::cpu::GuestCallCensus` is recorded per `Core` (`Core::guestCallCensus()`) and
`GuestCallCensus::log(why)` prints the run-end line with its denominators. Nothing in psxport calls
`log()`: the only callers of the census are `ResumableGuestCall::advance`'s completion path
(`recordCompleted`) and the accessors. A real 200-frame Crash 1 run at `scratch/struct-run4/leg4.log`
therefore contains no `guest-call` line at all, while `psxport/docs/codemap.md` states the census is
"reported once at run end".

## Why it matters

The census exists for one reason: "no call was ever long enough to need a resume" and "no resumable
call was ever made" must not look identical. Without the line, a title that migrates onto
`ResumableGuestCall` and never triggers it produces silence, which is exactly the ambiguity the
census was written to remove.

## Where the fix belongs

psxport, in the run-end path that already prints the Lightrec fallback telemetry and the guest
counters — `runtime/psx/native_boot.cpp`'s run-end reporting. One line:

```cpp
core->guestCallCensus().log("native boot");
```

## What this repository did instead

Nothing. `external/psxport` is not edited from this repository; Crash 1's own run-end reporting is
`Crash1Runtime::reportRun`, which logs only the projection counters; the Lightrec counters are the
framework's `run complete` line.