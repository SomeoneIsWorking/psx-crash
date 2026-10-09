#pragma once

#include "crash1_runtime.h"
#include "crash2_runtime.h"
#include "title_catalog.h"

#include <array>
#include <span>

namespace crash {

// The Crash trilogy as the multi-title host's catalog; index order is Crash 1, 2. Crash 3 joins when its runtime boots
// (issue 0018).
class CrashCatalog final : public psx::host::TitleCatalog {
public:
  CrashCatalog();

  std::string_view productName() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  GameRuntime &runtime(std::size_t index) const override;

private:
  std::array<psx::host::TitleIdentity, 2> identities_;
  // The host asks for a mutable runtime from a const catalog; the runtimes are the process's own.
  mutable crash1::Crash1Runtime crash1_;
  mutable crash2::Crash2Runtime crash2_;
};

} // namespace crash
