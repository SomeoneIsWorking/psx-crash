#pragma once

#include <cstdint>

namespace crash {

// A guest function's KSEG0 extent [begin, end), as the executable's facts measure it.
struct GuestFunctionRange {
  std::uint32_t begin;
  std::uint32_t end;

  [[nodiscard]] constexpr bool contains(std::uint32_t address) const {
    return address >= begin && address < end;
  }

  [[nodiscard]] constexpr bool valid() const {
    return begin != 0 && begin < end;
  }
};

} // namespace crash
