// Checks the guest-visible C0 table through the shipping B(56h) HLE.
#include "game.h"
#include "game_iface.h"

#include <cstdint>
#include <cstdio>
#include <memory>

#ifndef CRASH1_C0_EXCEPTION_HANDLER_SLOT
#error "CRASH1_C0_EXCEPTION_HANDLER_SLOT must come from titles/crash1/bios_contract.json"
#endif
#ifndef CRASH1_C0_EXCEPTION_HANDLER_ADDRESS
#error "CRASH1_C0_EXCEPTION_HANDLER_ADDRESS must come from titles/crash1/bios_contract.json"
#endif
#ifndef CRASH1_GET_C0_TABLE_FUNCTION
#error "CRASH1_GET_C0_TABLE_FUNCTION must come from titles/crash1/bios_contract.json"
#endif

int main() {
  static GameConfig config{};
  static const GameHooks hooks{};
  psxport_install_game(&config, &hooks);

  const auto game = std::make_unique<Game>();
  Core &core = game->core;
  if (!game->hle.dispatchBios('B', CRASH1_GET_C0_TABLE_FUNCTION)) {
    std::fputs("REFUSED: shipping HLE did not handle B(56h) GetC0Table\n", stderr);
    return 2;
  }

  const std::uint32_t c0Table = core.r[2];
  const std::uint32_t slotAddress = c0Table + CRASH1_C0_EXCEPTION_HANDLER_SLOT * sizeof(std::uint32_t);
  const std::uint32_t exceptionHandler = core.mem_r32(slotAddress);
  if (exceptionHandler != CRASH1_C0_EXCEPTION_HANDLER_ADDRESS) {
    std::fprintf(stderr,
                 "REFUSED: B(56h) returned C0 table 0x%08X, but slot %u at 0x%08X is "
                 "0x%08X; Crash requires retail C(06h) exception handler 0x%08X\n",
                 c0Table,
                 CRASH1_C0_EXCEPTION_HANDLER_SLOT,
                 slotAddress,
                 exceptionHandler,
                 CRASH1_C0_EXCEPTION_HANDLER_ADDRESS);
    return 2;
  }

  std::printf("PASS: B(56h) C0 slot %u owns retail exception handler 0x%08X\n",
              CRASH1_C0_EXCEPTION_HANDLER_SLOT,
              exceptionHandler);
  return 0;
}
