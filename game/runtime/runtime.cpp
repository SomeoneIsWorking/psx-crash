#include "runtime/runtime.h"

#include "boot/callback_boot.h"
#include "boot/gpu_watchdog.h"
#include "boot/libcd_init.h"
#include "cd_stock_read_completion.h"
#include "core.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "guest_code_module.h"
#include "native_dispatch.h"
#include "runtime/session.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash {

const TitleFacts &titleFacts(const Core &core) {
  const auto *const runtime = dynamic_cast<const Runtime *>(&core.runtime);
  if (runtime == nullptr) {
    lucent::error("crash-runtime", "a Crash native owner ran on a core with no Crash title runtime");
    std::abort();
  }
  return runtime->facts();
}

Runtime::Runtime(const TitleFacts &facts)
    : facts_(facts), libcd_(facts.stockLibcd, {}, 0),
      vsync_({.entry = facts.vsync.begin, .queryCounter = facts.vsyncQueryCounter}) {
  if (facts.padBuffers) {
    pad_.emplace(*facts.padBuffers);
  }
}

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{facts_.crt0}.run(core);
  // Retail C main does these before CoreLoop; dispatching C main itself would re-enter the guest loop
  // and run shutdown behind the host.
  psx::cpu::dispatchGuestToReturn(
      core, facts_.staticConstructors, psx::cpu::ExecutionBudget::currentTurn(core), "static constructors");
  core.mem_w32(facts_.useCdAddress, 1u);
  psx::cpu::dispatchGuestToReturn(core, facts_.init, psx::cpu::ExecutionBudget::currentTurn(core), "Init");
}

void Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libcd_.bind(game);
  libcd_init::registerOverride(game.core, facts_.libcdInit);
  callback_boot::registerOverride(game.core, facts_.callbackBoot);
  if (facts_.gpuWatchdog) {
    gpu_watchdog::registerOverrides(game.core, *facts_.gpuWatchdog);
  }
  FrameDriver::installOverrides(game, facts_.frame);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  return std::make_unique<Session>(facts_);
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(
      game, session(game.core).frameCut, facts_.frame, facts_.vsync.begin, FrameDriver::executeHostTurn);
}

void Runtime::deliverPad(Core &core, const psx::input::PadDelivery &delivery) {
  if (pad_) {
    pad_->deliver(core, delivery);
  }
}

void Runtime::stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) {
  if (facts_.codeModuleArena.empty()) {
    return;
  }
  psx::code_module::publishStockReadLanding(core, landing, facts_.codeModuleArena);
}

bool Runtime::sealedFrameIsCut(Core &core) const {
  return session(core).frameCut.isCut();
}

const psx::present::GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

} // namespace crash
