#include "crash_runtime.h"

#include "cd_stock_read_completion.h"
#include "core.h"
#include "crash_frame_driver.h"
#include "game.h"
#include "guest_code_module.h"
#include "native_dispatch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lucent/log.h>

namespace crash {

const TitleFacts &titleFacts(const Core &core) {
  const auto *const runtime = dynamic_cast<const CrashRuntime *>(core.runtime);
  if (runtime == nullptr) {
    lucent::error("crash-runtime", "a Crash native owner ran on a core with no Crash title runtime");
    std::abort();
  }
  return runtime->facts();
}

CrashRuntime::CrashRuntime(const TitleFacts &facts, GuestProjectionPublication &projection, std::string_view logDomain)
    : facts_(facts), projection_(projection), logDomain_(logDomain),
      platformPlan_(makeNativeFramePlatformPlan(facts.contract)),
      frameCut_(facts.frame.sceneIdAddress, facts.frame.sceneRequestAddress) {}

RenderCapabilities CrashRuntime::renderCapabilities() const {
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
      .temporalInterpolation = true,
  };
}

void *CrashRuntime::createContext(Core &) {
  return nullptr;
}

void CrashRuntime::destroyContext(void *) {}

void CrashRuntime::registerOverrides(Game &game) {
  libcd_init::registerOverride(game.core, facts_.libcdInit);
  stock_libcd::registerOverrides(game.core, facts_.stockLibcd);
  callback_boot::registerOverride(game.core, facts_.callbackBoot);
  gpu_watchdog::registerOverrides(game.core, facts_.gpuWatchdog);
  projection_.installSites(game.core);
  registerTitleOverrides(game);
  CrashFrameDriver::installOverrides(game, facts_.frame);
}

void CrashRuntime::bootInit(Core &core) {
  // Retail C main does these before CoreLoop; dispatching C main itself would re-enter the guest loop
  // and run shutdown behind the host.
  psx::cpu::dispatchGuestToReturn(
      core, facts_.staticConstructors, psx::cpu::ExecutionBudget::currentTurn(core), "static constructors");
  core.mem_w32(facts_.useCdAddress, 1u);
  psx::cpu::dispatchGuestToReturn(core, facts_.init, psx::cpu::ExecutionBudget::currentTurn(core), "Init");
}

const GuestProgramImage *CrashRuntime::guestProgramImage() const {
  return &facts_.image;
}

const PlatformHlePlan *CrashRuntime::platformHlePlan() const {
  return &platformPlan_;
}

const GuestPadBufferLayout *CrashRuntime::guestPadBufferLayout() const {
  return facts_.padBuffers;
}

bool CrashRuntime::guestVramIsPicture(const Game &) const {
  return false;
}

std::unique_ptr<FrameDriver> CrashRuntime::createFrameDriver(Game &game) {
  frameCut_.reset();
  return std::make_unique<CrashFrameDriver>(game, frameCut_, facts_.frame, facts_.contract);
}

bool CrashRuntime::sealedFrameIsCut(Core &) const {
  return frameCut_.isCut();
}

const GuestWidescreenProjection *CrashRuntime::guestWidescreenProjection() const {
  // Honest only with the three projection overrides from registerOverrides() installed.
  return &projection_;
}

void CrashRuntime::stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) {
  if (facts_.codeModuleArena.empty()) {
    return;
  }
  psx::code_module::publishStockReadLanding(core, landing, facts_.codeModuleArena);
}

bool CrashRuntime::controlCommand(Core &core, const char *cmd, const char *line, FILE *out) {
  if (std::strcmp(cmd, "warp") != 0) {
    return false;
  }
  auto *driver = dynamic_cast<CrashFrameDriver *>(core.game->frameDriver.get());
  std::fprintf(
      out, "%s\n", driver == nullptr ? "refused: no Crash frame driver is running" : driver->armWarp(line).c_str());
  return true;
}

const char *CrashRuntime::discEnvVar() const {
  return facts_.discEnvVar;
}

void CrashRuntime::reportRun(Core &) const {
  lucent::info(logDomain_,
               "guest projection run-end: centre_publications={} pass_throughs={} init_published={} "
               "last_retail_centre=({}, {}) published_H={} host_canvas={}x{} native={}x{}",
               projection_.publications(),
               projection_.passThroughs(),
               projection_.published() ? 1 : 0,
               projection_.retailCentre().x,
               projection_.retailCentre().y,
               projection_.publishedScreenDistance(),
               projection_.plan().presentationExtent.width,
               projection_.plan().presentationExtent.height,
               projection_.plan().nativeExtent.width,
               projection_.plan().nativeExtent.height);
}

} // namespace crash
