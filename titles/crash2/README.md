# Crash Bandicoot 2: Cortex Strikes Back

This title targets North American `SCUS-94154`, executable `SCUS_941.54`.
`executable.json` owns its exact hashes, PS-X EXE header, game-main, syscall, and VSync facts.

Recorded execution and the independent Mednafen CPU agree 34/34 at the first eight calls, including
game main `0x80049BD4` and `EnterCriticalSection` wrapper `0x80049D1C`. Cause `0x20`, EPC
`0x80049D20`, resume `0x80049D24`, and the following B0 dispatch agree; the B0 HLE and later
frame/gameplay path remain unverified.

The product runs this executable on the shared `crash::Runtime` with the addresses in
`entry/facts.cpp` and `executable.json`; it reuses no other title's address. See `../../docs/migration.md`.
