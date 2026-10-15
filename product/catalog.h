#pragma once

#include "crash1/entry/runtime.h"
#include "crash2/entry/runtime.h"
#include "crash3/entry/runtime.h"
#include "title_catalog.h"

#include <array>
#include <filesystem>
#include <memory>
#include <span>

namespace crash {

// The Crash trilogy as the multi-title host's catalog; index order is Crash 1, 2, 3.
class Catalog final : public psx::host::TitleCatalog {
public:
  Catalog();

  std::string_view productName() const override;
  std::filesystem::path provisioningRoot() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  psx::game::GameRuntime &runtime(std::size_t index) const override;

private:
  std::array<psx::host::TitleIdentity, 3> identities_;
  // The host asks for a mutable runtime from a const catalog; the runtimes are the process's own.
  mutable crash1::Runtime crash1_;
  mutable crash2::Runtime crash2_;
  mutable crash3::Runtime crash3_;
};

// The product's catalog, run by the framework main (psxport_add_product).
std::unique_ptr<psx::host::TitleCatalog> createCatalog();

} // namespace crash
