#include "crash1_bios_pad_input.h"

#include "core.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

bool expectWord(Core &core,
                std::uint16_t activeLowButtons,
                std::uint32_t expected,
                std::uint16_t expectedPrimaryButtons,
                const char *label) {
  crash1::bios_pad_input::publishPrimary(core, activeLowButtons);
  std::uint32_t actual = core.mem_r32(crash1::bios_pad_input::wordAddress());
  if (actual != expected) {
    std::fprintf(stderr, "%s: expected 0x%08X, got 0x%08X\n", label, expected, actual);
    return false;
  }
  // PadUpdate selects the low halfword for controller 0, the high one for controller 1.
  std::uint32_t logicalButtons = ~actual;
  if (static_cast<std::uint16_t>(logicalButtons) != expectedPrimaryButtons ||
      static_cast<std::uint16_t>(logicalButtons >> 16u) != 0u) {
    std::fprintf(stderr,
                 "%s: controller-0/1 logical buttons are 0x%04X/0x%04X, expected 0x%04X/0x0000\n",
                 label,
                 static_cast<std::uint16_t>(logicalButtons),
                 static_cast<std::uint16_t>(logicalButtons >> 16u),
                 expectedPrimaryButtons);
    return false;
  }
  return true;
}

} // namespace

int main() {
  auto core = std::make_unique<Core>();
  bool ok = true;

  ok &= expectWord(*core, 0xFFFFu, 0xFFFFFFFFu, 0x0000u, "released input");
  ok &= expectWord(*core, 0xBFFFu, 0xFFFFFFBFu, 0x0040u, "Cross input");
  ok &= expectWord(*core, 0xFFF7u, 0xFFFFF7FFu, 0x0800u, "Start input");

  return ok ? 0 : 1;
}
