#include "crash1_catalog.h"

#include <array>
#include <cstdlib>

#include <lucent/log.h>

namespace crash1 {
namespace {

constexpr std::array<psx::host::TitleIdentity, 1> kIdentities{{
    psx::host::TitleIdentity{
        .displayName = CRASH1_EXE_TITLE,
        .serial = CRASH1_EXE_NAME,
        .slug = "crash1",
        .fileSize = CRASH1_EXE_EXPECTED_SIZE,
        .sha256 = CRASH1_EXE_SHA256,
        .entry = CRASH1_EXE_ENTRY,
        .globalPointer = CRASH1_EXE_GP,
        .textAddress = CRASH1_EXE_TEXT_ADDRESS,
        .textSize = CRASH1_EXE_TEXT_SIZE,
        .stackAddress = CRASH1_EXE_STACK_ADDRESS,
        .stackOffset = CRASH1_EXE_STACK_OFFSET,
    },
}};

} // namespace

std::string_view Crash1Catalog::productName() const {
  return "Crash Bandicoot";
}

std::span<const psx::host::TitleIdentity> Crash1Catalog::titles() const {
  return kIdentities;
}

Crash1Runtime &Crash1Catalog::runtime(std::size_t index) const {
  if (index >= kIdentities.size()) {
    lucent::error("crash1-catalog", "no runtime for catalog index {}", index);
    std::abort();
  }
  return runtime_;
}

} // namespace crash1
