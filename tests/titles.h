// titles.h — each Crash title's Game around its own Runtime, for tests of the shared owners.
#pragma once

#include "core.h"
#include "crash1/entry/runtime.h"
#include "crash2/entry/runtime.h"
#include "crash3/entry/runtime.h"
#include "game.h"
#include "typed_config.h"

#include <cstdio>
#include <memory>

namespace crash_test {

template <typename TitleRuntime> class TitleGame {
public:
  // On the heap: a Game holds the whole PSX address space.
  TitleGame() : game_(std::make_unique<Game>(runtime_, psx::config::Config{})) {}

  Game &game() {
    return *game_;
  }
  Core &core() {
    return game_->core;
  }
  TitleRuntime &runtime() {
    return runtime_;
  }

private:
  TitleRuntime runtime_;
  std::unique_ptr<Game> game_;
};

template <typename TitleRuntime, typename Body> int runTitle(const char *name, Body &body) {
  auto title = std::make_unique<TitleGame<TitleRuntime>>();
  return body(name, title->game(), title->runtime().facts());
}

// Runs `body(name, game, facts)` once per Crash title, each in a fresh Game around that title's runtime.
template <typename Body> int forEachTitle(Body &&body) {
  return runTitle<crash1::Runtime>("crash1", body) + runTitle<crash2::Runtime>("crash2", body) +
         runTitle<crash3::Runtime>("crash3", body);
}

inline int fail(const char *title, const char *detail) {
  std::fprintf(stderr, "FAIL %s: %s\n", title, detail);
  return 1;
}

} // namespace crash_test
