#include "catalog.h"

#include "crash1/entry/facts.h"
#include "crash2/entry/facts.h"
#include "crash3/entry/facts.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash {

Catalog::Catalog() : identities_{crash1::identity(), crash2::identity(), crash3::identity()} {}

std::string_view Catalog::productName() const {
  return "Crash Bandicoot";
}

std::filesystem::path Catalog::provisioningRoot() const {
  return "scratch/bin";
}

std::span<const psx::host::TitleIdentity> Catalog::titles() const {
  return identities_;
}

psx::game::GameRuntime &Catalog::runtime(std::size_t index) const {
  switch (index) {
  case 0:
    return crash1_;
  case 1:
    return crash2_;
  case 2:
    return crash3_;
  default:
    lucent::error("crash-catalog", "no runtime for catalog index {}", index);
    std::abort();
  }
}

std::unique_ptr<psx::host::TitleCatalog> createCatalog() {
  return std::make_unique<Catalog>();
}

} // namespace crash
