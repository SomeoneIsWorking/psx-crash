#pragma once

#include "runtime/runtime.h"

namespace crash2 {

// Crash 2: the shared Crash runtime over its facts, its draw replay and poses included.
class Runtime final : public crash::Runtime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  bool interpolates() const override {
    return true;
  }
};

} // namespace crash2
