#pragma once

#include "runtime/runtime.h"

namespace crash2 {

// Crash 2: the shared Crash runtime over its facts.
class Runtime final : public crash::Runtime {
public:
  Runtime();
  bool interpolates() const override {
    return true;
  }
};

} // namespace crash2
