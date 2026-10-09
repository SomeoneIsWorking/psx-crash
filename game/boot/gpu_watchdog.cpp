#include "gpu_watchdog.h"

#include "core.h"
#include "game.h"
#include "native_dispatch.h"
#include "title_facts.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash::gpu_watchdog {
namespace {

constexpr std::uint32_t kDisplayTimeoutFields = 240u;
constexpr std::uint32_t kPollLimit = 0xF0000u;

// Return addresses inside the retail check body, from its entry.
constexpr std::uint32_t kFirstLogReturn = 0xA0u;
constexpr std::uint32_t kSecondLogReturn = 0xCCu;
constexpr std::uint32_t kEnterCriticalReturn = 0xD4u;
constexpr std::uint32_t kExitCriticalReturn = 0x150u;

void startOverride(Core *core) {
  start(*core, titleFacts(*core).gpuWatchdog);
}

void checkOverride(Core *core) {
  check(*core, titleFacts(*core).gpuWatchdog, [](Core *target, std::uint32_t address) {
    psx::cpu::dispatchGuestToReturn(
        *target, address, psx::cpu::ExecutionBudget::currentTurn(*target), "GPU watchdog leaf");
  });
}

void dispatchAt(Core &core, MainDispatch dispatch, std::uint32_t address, std::uint32_t continuation) {
  core.r[31] = continuation;
  dispatch(&core, address);
}

} // namespace

void registerOverrides(Core &core, const Program &program) {
  const struct Binding {
    std::uint32_t address;
    psx::cpu::NativeFunction function;
    const char *name;
  } bindings[]{
      {program.start.begin, startOverride, "GPU watchdog start"},
      {program.check.begin, checkOverride, "GPU watchdog check"},
  };
  for (const Binding &binding : bindings) {
    psx::cpu::installNativeOverride(core, binding.address, binding.name, binding.function);
  }
}

void start(Core &core, const Program &program) {
  // Retail uses VSync(-1) only to read the field count; Timing::frameTick owns that counter.
  core.mem_w32(program.deadline, core.game->timing.vblank + kDisplayTimeoutFields);
  core.mem_w32(program.pollCount, 0u);
  core.r[2] = 0u;
}

void check(Core &core, const Program &program, MainDispatch dispatch) {
  const std::uint32_t pollCount = core.mem_r32(program.pollCount);
  core.mem_w32(program.pollCount, pollCount + 1u);
  const bool displayExpired =
      static_cast<std::int32_t>(core.mem_r32(program.deadline)) < static_cast<std::int32_t>(core.game->timing.vblank);
  if (!displayExpired && pollCount <= kPollLimit) {
    core.r[2] = 0u;
    return;
  }

  if (dispatch == nullptr) {
    lucent::error("crash-gpu-watchdog", "GPU watchdog timeout owner received a null main dispatcher");
    std::abort();
  }

  const std::uint32_t entry = program.check.begin;
  const std::uint32_t incomingStack = core.r[29];
  const std::uint32_t incomingReturn = core.r[31];
  core.r[29] -= 32u;
  core.mem_w32(core.r[29] + 24u, incomingReturn);

  core.r[4] = program.diagnosticFormat;
  core.r[5] = (core.mem_r32(program.queueWrite) - core.mem_r32(program.queueRead)) & 63u;
  core.r[6] = core.mem_r32(core.mem_r32(program.gp0Pointer));
  core.r[7] = core.mem_r32(core.mem_r32(program.gp1Pointer));
  core.mem_w32(core.r[29] + 16u, core.mem_r32(core.mem_r32(program.statusPointer)));
  dispatchAt(core, dispatch, program.log, entry + kFirstLogReturn);

  core.r[4] = program.callbackFormat;
  core.r[5] = core.mem_r32(program.lastCallback);
  core.r[6] = core.mem_r32(program.lastPayload);
  core.r[7] = core.mem_r32(program.lastArgument);
  dispatchAt(core, dispatch, program.log, entry + kSecondLogReturn);

  core.r[4] = 0u;
  dispatchAt(core, dispatch, program.critical, entry + kEnterCriticalReturn);
  core.mem_w32(program.criticalToken, core.r[2]);
  core.mem_w32(program.queueRead, 0u);
  core.mem_w32(program.queueWrite, 0u);
  core.mem_w32(core.mem_r32(program.gp1Pointer), 0x401u);
  const std::uint32_t dmaControl = core.mem_r32(program.dmaControlPointer);
  core.mem_w32(dmaControl, core.mem_r32(dmaControl) | 0x800u);
  const std::uint32_t gpuGp0 = core.mem_r32(program.gp0Pointer);
  core.mem_w32(gpuGp0, 0x02000000u);
  core.mem_w32(gpuGp0, 0x01000000u);
  core.r[4] = core.mem_r32(program.criticalToken);
  dispatchAt(core, dispatch, program.critical, entry + kExitCriticalReturn);

  core.r[2] = 0xFFFFFFFFu;
  core.r[31] = core.mem_r32(core.r[29] + 24u);
  core.r[29] = incomingStack;
}

} // namespace crash::gpu_watchdog
