#include "boundary_runtime.h"

#include <lucent/log.h>

#include <cstdlib>

namespace crash {

BoundaryRuntime::BoundaryRuntime(std::string_view logDomain, std::string_view blockedReason)
    : logDomain_(logDomain), blockedReason_(blockedReason) {}

RenderCapabilities BoundaryRuntime::renderCapabilities() const {
  return RenderCapabilities::widescreenOnly();
}

void *BoundaryRuntime::createContext(Core &) {
  return nullptr;
}

void BoundaryRuntime::destroyContext(void *) {}

void BoundaryRuntime::registerOverrides(Game &) {}

void BoundaryRuntime::bootInit(Core &) {
  lucent::error(logDomain_, "{}", blockedReason_);
  std::abort();
}

} // namespace crash
