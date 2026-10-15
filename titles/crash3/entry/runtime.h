#pragma once

#include "runtime/runtime.h"

namespace crash3 {

// Crash 3: the shared Crash runtime over its facts.
class Runtime final : public crash::Runtime {
public:
  Runtime();
};

} // namespace crash3
