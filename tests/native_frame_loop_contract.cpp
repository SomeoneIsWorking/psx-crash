#include "native_frame_loop_contract.h"
#include "crash1_runtime.h"
#include "crash2_runtime.h"
#include "crash3_runtime.h"
#include "execution_control.h"
#include "game.h"
#include "game_runtime.h"
#include "platform_hle.h"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int gFailures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", detail);
    ++gFailures;
  }
}

template <typename Runtime>
void checkContract(const char *name,
                   std::uint32_t expectedVSync,
                   std::uint32_t expectedVSyncEnd,
                   crash::NativeFrameLoopState expectedState) {
  Runtime runtime;
  const crash::NativeFrameLoopContract &frame = runtime.nativeFrameLoopContract();
  const PlatformHlePlan *platform = runtime.platformHlePlan();
  check(frame.guestVSync.begin == expectedVSync, name);
  check(frame.guestVSync.end == expectedVSyncEnd, name);
  check(frame.state == expectedState, name);
  check(frame.canStepFrame() == (expectedState == crash::NativeFrameLoopState::Ready), name);
  check(platform != nullptr, name);
  if (platform != nullptr) {
    check(platform->vsyncAddress == expectedVSync, name);
    check(platform->windowLo[0] == expectedVSync, name);
    check(platform->windowHi[0] == expectedVSyncEnd, name);
    check(platform->bindingCount == 0, name);
  }
}

template <typename Runtime, typename Operation> void expectAbort(const char *name, Operation operation) {
  const pid_t pid = fork();
  if (pid < 0) {
    std::perror("fork");
    ++gFailures;
    return;
  }
  if (pid == 0) {
    const rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
    Runtime runtime;
    psxport_install_game(runtime);
    auto game = std::make_unique<Game>();
    operation(runtime, *game);
    _exit(EXIT_SUCCESS);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) != pid) {
    std::perror("waitpid");
    ++gFailures;
    return;
  }
  const bool aborted = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
  check(aborted, name);
  std::printf("%s: %s\n", aborted ? "PASS" : "FAIL", name);
}

template <typename Runtime> void checkVSyncExit(const char *name) {
  Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  crash::initializeNativeFrameLoopContract(*game);
  OverrideFn vsync = game->platform_hle.lookup(runtime.nativeFrameLoopContract().guestVSync.begin);
  check(vsync != nullptr, name);
  if (vsync == nullptr) {
    return;
  }
  // A negative VSync argument queries the field count: no field advance, no frame-boundary exit.
  constexpr std::uint32_t kProbeCount = 0x1234u;
  const std::uint32_t counter = runtime.nativeFrameLoopContract().vsyncQueryCounter;
  check(counter != 0, name);
  game->core.mem_w32(counter, kProbeCount);

  game->core.r[4] = 0xFFFFFFFFu; // VSync(-1)
  vsync(&game->core);
  check(!game->core.executionControl().consume().has_value(), name);
  check(game->core.r[2] == kProbeCount, name);
  check(game->core.mem_r32(counter) == kProbeCount, name);

  game->core.r[4] = 0u; // VSync(0)
  vsync(&game->core);
  const auto waited = game->core.executionControl().consume();
  check(waited.has_value(), name);
  if (waited) {
    check(waited->reason == psx::cpu::ExecutionExitReason::FrameBoundary, name);
  }
}

template <typename Runtime> void checkRefusingFrameDeath(const char *name) {
  expectAbort<Runtime>(name, [](Runtime &, Game &game) {
    game.frameDriver->stepFrame(game.core, 0);
  });
}

} // namespace

int main() {
  checkContract<crash1::Crash1Runtime>(
      "Crash 1 contract", 0x8003E4F0u, 0x8003E638u, crash::NativeFrameLoopState::FiniteBootSeamOnly);
  checkContract<crash2::Crash2Runtime>(
      "Crash 2 contract", 0x8004A484u, 0x8004A5CCu, crash::NativeFrameLoopState::Missing);
  checkContract<crash3::Crash3Runtime>(
      "Crash 3 contract", 0x8004B2A8u, 0x8004B3F0u, crash::NativeFrameLoopState::Missing);

  checkVSyncExit<crash1::Crash1Runtime>("Crash 1 guest VSync typed exit");
  checkRefusingFrameDeath<crash2::Crash2Runtime>("Crash 2 unproven frame aborts");
  checkVSyncExit<crash2::Crash2Runtime>("Crash 2 guest VSync typed exit");
  checkRefusingFrameDeath<crash3::Crash3Runtime>("Crash 3 unproven frame aborts");
  checkVSyncExit<crash3::Crash3Runtime>("Crash 3 guest VSync typed exit");

  std::printf("native frame contract: %s\n", gFailures == 0 ? "PASS" : "FAIL");
  return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
