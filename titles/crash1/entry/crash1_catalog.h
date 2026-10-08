#pragma once

#include "crash1_runtime.h"
#include "title_catalog.h"

#include <span>

namespace crash1 {

// Crash 1 as the multi-title host's catalog. Crash 2 and 3 join it when their runtimes boot (issues 0017, 0018).
class Crash1Catalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  Crash1Runtime &runtime(std::size_t index) const override;

private:
  // The host asks for a mutable runtime from a const catalog; the runtime is the process's one.
  mutable Crash1Runtime runtime_;
};

} // namespace crash1
