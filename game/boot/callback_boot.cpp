#include "callback_boot.h"

#include "core.h"
#include "native_dispatch.h"
#include "title_facts.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash::callback_boot {
namespace {

struct EventSpec {
  std::uint32_t descriptor;
  std::uint32_t mode;
};

constexpr std::array<EventSpec, kEventCount> kEvents{{
    {0xF4000001u, 0x0004u},
    {0xF4000001u, 0x8000u},
    {0xF4000001u, 0x0100u},
    {0xF4000001u, 0x2000u},
    {0xF0000011u, 0x0004u},
    {0xF0000011u, 0x8000u},
    {0xF0000011u, 0x0100u},
    {0xF0000011u, 0x2000u},
}};

// Return addresses inside the retail body, from its entry.
constexpr std::uint32_t kEnterReturn = 0x10u;
constexpr std::uint32_t kFirstOpenReturn = 0x28u;
constexpr std::uint32_t kOpenReturnStride = 0x1Cu;
constexpr std::uint32_t kExitReturn = 0xF8u;
constexpr std::uint32_t kInitializePadReturn = 0x108u;
constexpr std::uint32_t kStartPadReturn = 0x118u;
constexpr std::uint32_t kChangeClearPadReturn = 0x128u;
constexpr std::uint32_t kFirstCloseReturn = 0x13Cu;
constexpr std::uint32_t kCloseReturnStride = 0xCu;

void initializeOverride(Core *core) {
  initializeDriver(*core, titleFacts(*core).callbackBoot, [](Core *target, std::uint32_t address) {
    psx::cpu::dispatchGuestToReturn(
        *target, address, psx::cpu::ExecutionBudget::currentTurn(*target), "callback initialization leaf");
  });
}

void dispatchAt(Core &core, MainDispatch dispatch, std::uint32_t address, std::uint32_t continuation) {
  core.r[31] = continuation;
  dispatch(&core, address);
}

} // namespace

void registerOverride(Core &core, const Program &program) {
  psx::cpu::installNativeOverride(core, program.initialize.begin, "callback initialization", initializeOverride);
}

void initializeDriver(Core &core, const Program &program, MainDispatch dispatch) {
  if (dispatch == nullptr) {
    lucent::error("crash-callback", "callback boot owner received a null main dispatcher");
    std::abort();
  }

  // The retail body creates eight BIOS events, inits the pad service, closes the handles.
  // Its four VSync(5) calls are hardware-settle delays and are skipped.
  const std::uint32_t entry = program.initialize.begin;
  const std::uint32_t incomingStack = core.r[29];
  const std::uint32_t incomingReturn = core.r[31];
  core.r[29] -= 24u;
  core.mem_w32(core.r[29] + 16u, incomingReturn);

  dispatchAt(core, dispatch, program.enterCritical, entry + kEnterReturn);
  for (std::size_t index = 0; index < kEvents.size(); ++index) {
    core.r[4] = kEvents[index].descriptor;
    core.r[5] = kEvents[index].mode;
    core.r[6] = 0x2000u;
    core.r[7] = 0u;
    dispatchAt(core, dispatch, program.openEvent, entry + kFirstOpenReturn + kOpenReturnStride * index);
    core.mem_w32(core.r[28] + program.handleOffsets[index], core.r[2]);
  }
  dispatchAt(core, dispatch, program.exitCritical, entry + kExitReturn);

  core.r[4] = 1u;
  dispatchAt(core, dispatch, program.initializePad, entry + kInitializePadReturn);
  dispatchAt(core, dispatch, program.startPad, entry + kStartPadReturn);
  dispatchAt(core, dispatch, program.changeClearPad, entry + kChangeClearPadReturn);

  for (std::size_t index = 0; index < kEvents.size(); ++index) {
    core.r[4] = core.mem_r32(core.r[28] + program.handleOffsets[index]);
    dispatchAt(core, dispatch, program.closeEvent, entry + kFirstCloseReturn + kCloseReturnStride * index);
  }

  core.r[31] = core.mem_r32(core.r[29] + 16u);
  core.r[29] = incomingStack;
}

} // namespace crash::callback_boot
