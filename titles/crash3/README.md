# Crash Bandicoot: Warped

This title targets North American `SCUS-94244`, executable `SCUS_942.44`. The retail disc's
`SYSTEM.CNF` selects that file; the unrelated `DRAGON/SPYRO.EXE` on the same disc is never accepted as
the product image. `executable.json` owns the measured identity and PS-X EXE facts.

Recorded execution and the independent CPU agree 34/34 at the first eight calls through game main
`0x80048AA0` and `EnterCriticalSection` wrapper `0x80048C38`. Cause `0x20`, EPC `0x80048C3C`,
resume `0x80048C40`, and the following B0 dispatch agree. Structural target `0x800112B8` is not an
A(39h) thunk; that modeled return is refused.

These facts are inputs to the later native/Lightrec product, not instructions to extend the static
route. Crash 3 migration begins only after Crash 2's gameplay gate and uses its own authenticated
image and addresses. See `../../docs/migration.md`.
