#pragma once

#include "crash1_runtime.h"
#include "crash2_runtime.h"
#include "game_runtime.h"

#include <cstdio>

namespace crash_test {

// Runs `body(name, facts)` once per title that ships the shared Crash runtime, with that title's runtime installed.
template <typename Body> int forEachTitle(Body &&body) {
  int failures = 0;
  {
    crash1::Crash1Runtime runtime;
    psxport_install_game(runtime);
    failures += body("crash1", runtime.facts());
  }
  {
    crash2::Crash2Runtime runtime;
    psxport_install_game(runtime);
    failures += body("crash2", runtime.facts());
  }
  return failures;
}

inline int fail(const char *title, const char *detail) {
  std::fprintf(stderr, "FAIL %s: %s\n", title, detail);
  return 1;
}

} // namespace crash_test
