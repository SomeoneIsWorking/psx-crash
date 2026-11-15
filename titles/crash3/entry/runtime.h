#pragma once

#include "runtime/runtime.h"

namespace crash3 {

// Crash 3: the shared Crash runtime over its facts, its draw replay included.
class Runtime final : public crash::Runtime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  bool interpolates() const override {
    return true;
  }
};

} // namespace crash3
