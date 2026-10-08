#include "crash1_runtime.h"

#include "core.h"
#include "crash1_callback_boot.h"
#include "crash1_cd_boot.h"
#include "crash1_disc_index_io.h"
#include "crash1_frame_driver.h"
#include "crash1_gpu_watchdog.h"
#include "crash1_horizontal_bound.h"
#include "game.h"
#include "gpu_vk.h"
#include "native_dispatch.h"
#include "platform_hle.h"

#include <lucent/log.h>
#include <memory>

#ifndef CRASH1_STATIC_CONSTRUCTORS_ENTRY
#error "CRASH1 native boot facts must come from titles/crash1/executable.json"
#endif

namespace crash1 {

const GuestProgramImage Crash1Runtime::programImage_{
    .bss = {0x80056598u, 0x80061A78u},
    .stackTopWordAddress = 0x80056408u,
    .stackReserveWordAddress = 0x80056404u,
    .heapBase = 0x80061A78u,
    .heapSizeStoreAddress = 0x800538F0u,
    .heapBaseStoreAddress = 0x800538ECu,
    .globalPointer = 0x800563FCu,
    .libcInitEntry = 0x80011A18u,
    .gameMainEntry = 0x8003E0C0u,
    .crt0Entry = 0x8003E018u,
    .residentText = {0x00010000u, 0x00056800u},
    .backtraceText = {},
    .stackBias = {true, -8},
};

const PlatformHlePlan Crash1Runtime::platformPlan_ = crash::makeNativeFramePlatformPlan(Crash1FrameDriver::contract());

Crash1Runtime::Crash1Runtime() = default;

RenderCapabilities Crash1Runtime::renderCapabilities() const {
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
      .temporalInterpolation = true,
  };
}

void *Crash1Runtime::createContext(Core &) {
  return nullptr;
}

void Crash1Runtime::destroyContext(void *) {}

void Crash1Runtime::registerOverrides(Game &game) {
  cd_boot::registerOverride(game.core);
  disc_index_io::registerOverrides(game.core);
  callback_boot::registerOverride(game.core);
  gpu_watchdog::registerOverrides(game.core);
  widescreen_.installSites(game.core);
  installCrash1HorizontalBound(game.core);
  installCrash1BlockPool(game.core);
  Crash1FrameDriver::installOverrides(game);
}

void Crash1Runtime::bootInit(Core &core) {
  const std::uint32_t entries[]{CRASH1_STATIC_CONSTRUCTORS_ENTRY, CRASH1_INIT_ENTRY};

  // Retail C main 0x80011D88 does these before CoreLoop 0x80011FC4; dispatching C main itself would
  // re-enter the guest loop and run shutdown behind the host.
  psx::cpu::dispatchGuestToReturn(
      core, entries[0], psx::cpu::ExecutionBudget::currentTurn(core), "Crash 1 static constructors");
  core.mem_w32(CRASH1_USE_CD_ADDRESS, 1u);
  psx::cpu::dispatchGuestToReturn(core, entries[1], psx::cpu::ExecutionBudget::currentTurn(core), "Crash 1 Init");
}

const GuestProgramImage *Crash1Runtime::guestProgramImage() const {
  return &programImage_;
}

const PlatformHlePlan *Crash1Runtime::platformHlePlan() const {
  return &platformPlan_;
}

const GuestWidescreenProjection *Crash1Runtime::guestWidescreenProjection() const {
  // Honest only with the three projection overrides from registerOverrides() installed.
  return &widescreen_;
}

const char *Crash1Runtime::discEnvVar() const {
  return "PSXPORT_CRASH1_DISC";
}

void Crash1Runtime::reportRun(Core &) const {
  lucent::info("crash1",
               "guest projection run-end: centre_publications={} pass_throughs={} init_published={} "
               "last_retail_centre=({}, {}) published_H={} host_canvas={}x{} native={}x{}",
               widescreen_.publications(),
               widescreen_.passThroughs(),
               widescreen_.published() ? 1 : 0,
               widescreen_.retailCentre().x,
               widescreen_.retailCentre().y,
               widescreen_.publishedScreenDistance(),
               widescreen_.plan().presentationExtent.width,
               widescreen_.plan().presentationExtent.height,
               widescreen_.plan().nativeExtent.width,
               widescreen_.plan().nativeExtent.height);
}

bool Crash1Runtime::guestVramIsPicture(const Game &) const {
  return false;
}

std::unique_ptr<FrameDriver> Crash1Runtime::createFrameDriver(Game &game) {
  // A new session's first record is a cut.
  frameCut_ = Crash1FrameCut{};
  return std::make_unique<Crash1FrameDriver>(game, frameCut_);
}

bool Crash1Runtime::sealedFrameIsCut(Core &) const {
  return frameCut_.isCut();
}

const crash::NativeFrameLoopContract &Crash1Runtime::nativeFrameLoopContract() const {
  return Crash1FrameDriver::contract();
}

} // namespace crash1
