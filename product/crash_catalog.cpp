#include "crash_catalog.h"

#include "crash1_facts.h"
#include "crash2_facts.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash {

CrashCatalog::CrashCatalog() : identities_{crash1::identity(), crash2::identity()} {}

std::string_view CrashCatalog::productName() const {
  return "Crash Bandicoot";
}

std::span<const psx::host::TitleIdentity> CrashCatalog::titles() const {
  return identities_;
}

GameRuntime &CrashCatalog::runtime(std::size_t index) const {
  switch (index) {
  case 0:
    return crash1_;
  case 1:
    return crash2_;
  default:
    lucent::error("crash-catalog", "no runtime for catalog index {}", index);
    std::abort();
  }
}

} // namespace crash
