#include "crash1_facts.h"

#ifndef CRASH_STATIC_CONSTRUCTORS_ENTRY
#error "Crash 1 native facts must come from titles/crash1/executable.json"
#endif

namespace crash1 {
namespace {

constexpr GuestProgramImage kImage{
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

// libcd state reset by CD_init 0x80044E8C (decompiled): callbacks, status, last command, 10-word workspace.
constexpr crash::libcd_init::Program kLibcdInit{
    .initialize = {CRASH_CD_INITIALIZE_ENTRY, CRASH_CD_INITIALIZE_END},
    .lastSyncCallback = 0x800555A0u,
    .lastReadyCallback = 0x800555A4u,
    .lastStatus = 0x800555B0u,
    .lastResult = 0x800555B4u,
    .lastCommand = 0x800555C0u,
    .pendingCommand = 0x800555C1u,
    .commandWorkspace = 0x80055880u,
    .commandWorkspaceWords = 10u,
    .syncStatus = 0x8005587Cu,
};

constexpr crash::stock_libcd::Program kStockLibcd{
    .control = {CRASH_CD_CONTROL_ENTRY, CRASH_CD_CONTROL_END},
    .controlF = {CRASH_CD_CONTROL_F_ENTRY, CRASH_CD_CONTROL_F_END},
    .syncWrapper = {CRASH_CD_SYNC_WRAPPER_ENTRY, CRASH_CD_SYNC_WRAPPER_END},
    .sync = {CRASH_CD_SYNC_ENTRY, CRASH_CD_SYNC_END},
    .read = {CRASH_CD_READ_ENTRY, CRASH_CD_READ_END},
    .readSync = {CRASH_CD_READ_SYNC_ENTRY, CRASH_CD_READ_SYNC_END},
};

// BIOS leaves FUN_8003CB9C calls: EnterCriticalSection, OpenEvent, ExitCriticalSection, CloseEvent,
// InitCARD(1), StartCARD, ChangeClearPad; the handles land in gp-relative slots.
constexpr crash::callback_boot::Program kCallbackBoot{
    .initialize = {CRASH_CALLBACK_INITIALIZE_ENTRY, CRASH_CALLBACK_INITIALIZE_END},
    .enterCritical = 0x8003E1F8u,
    .openEvent = 0x8003E1A8u,
    .exitCritical = 0x8003E208u,
    .closeEvent = 0x8003E1D8u,
    .initializePad = 0x80051444u,
    .startPad = 0x80051454u,
    .changeClearPad = 0x8003E198u,
    .handleOffsets = {712u, 716u, 720u, 724u, 752u, 756u, 764u, 768u},
};

constexpr crash::gpu_watchdog::Program kGpuWatchdog{
    .start = {CRASH_GPU_WATCHDOG_START_ENTRY, CRASH_GPU_WATCHDOG_START_END},
    .check = {CRASH_GPU_WATCHDOG_CHECK_ENTRY, CRASH_GPU_WATCHDOG_CHECK_END},
    .deadline = 0x80054B84u,
    .pollCount = 0x80054B88u,
    .queueRead = 0x80054B74u,
    .queueWrite = 0x80054B70u,
    .gp0Pointer = 0x80054B40u,
    .statusPointer = 0x80054B44u,
    .gp1Pointer = 0x80054B4Cu,
    .dmaControlPointer = 0x80054B5Cu,
    .lastCallback = 0x80054B60u,
    .lastPayload = 0x80054B64u,
    .lastArgument = 0x80054B68u,
    .criticalToken = 0x80054B80u,
    .diagnosticFormat = 0x80011350u,
    .callbackFormat = 0x80011384u,
    .log = 0x8003D730u,
    .critical = 0x8003E870u,
};

constexpr crash::FrameProgram kFrame{
    .coreLoop = {CRASH_CORE_LOOP_ENTRY, CRASH_CORE_LOOP_END},
    .initialScene = CRASH_INITIAL_SCENE,
    .iteration = {CRASH_FRAME_ITERATION_ENTRY, CRASH_FRAME_ITERATION_END},
    .transition = {CRASH_FRAME_TRANSITION_ENTRY, CRASH_FRAME_TRANSITION_END},
    .transitionReturn = CRASH_FRAME_TRANSITION_RETURN,
    .gpuUpdate = {CRASH_GPU_UPDATE_ENTRY, CRASH_GPU_UPDATE_END},
    .firstVSync = CRASH_FIRST_VSYNC,
    .afterFirstVSync = CRASH_AFTER_FIRST_VSYNC,
    .secondVSync = CRASH_SECOND_VSYNC,
    .afterVSync = CRASH_AFTER_VSYNC,
    .doneAddress = CRASH_DONE_ADDRESS,
    .sceneIdAddress = CRASH_SCENE_ID_ADDRESS,
    .sceneRequestAddress = CRASH_SCENE_REQUEST_ADDRESS,
    .rootCounterIncrement = CRASH_ROOT_COUNTER_INCREMENT,
    .setRootCounter = CRASH_SET_ROOT_COUNTER,
    .startRootCounter = CRASH_START_ROOT_COUNTER,
    .stopRootCounter = CRASH_STOP_ROOT_COUNTER,
};

constexpr crash::NativeFrameLoopContract kContract{
    .codeword = "SCUS-94900",
    .guestVSync = {CRASH_TITLE_VSYNC_ENTRY, CRASH_TITLE_VSYNC_END},
    .vsyncQueryCounter = CRASH_TITLE_VSYNC_QUERY_COUNTER,
    .state = crash::NativeFrameLoopState::FiniteBootSeamOnly,
    .refusal = "candidate frame step is not ready until a real product frame returns",
};

constexpr psx::host::TitleIdentity kIdentity{
    .displayName = CRASH_EXE_TITLE,
    .serial = CRASH_EXE_NAME,
    .slug = "crash1",
    .fileSize = CRASH_EXE_EXPECTED_SIZE,
    .sha256 = CRASH_EXE_SHA256,
    .entry = CRASH_EXE_ENTRY,
    .globalPointer = CRASH_EXE_GP,
    .textAddress = CRASH_EXE_TEXT_ADDRESS,
    .textSize = CRASH_EXE_TEXT_SIZE,
    .stackAddress = CRASH_EXE_STACK_ADDRESS,
    .stackOffset = CRASH_EXE_STACK_OFFSET,
};

const crash::TitleFacts kFacts{
    .discEnvVar = "PSXPORT_CRASH1_DISC",
    .image = kImage,
    .staticConstructors = CRASH_STATIC_CONSTRUCTORS_ENTRY,
    .useCdAddress = CRASH_USE_CD_ADDRESS,
    .init = CRASH_INIT_ENTRY,
    .libcdInit = kLibcdInit,
    .stockLibcd = kStockLibcd,
    .callbackBoot = kCallbackBoot,
    .gpuWatchdog = kGpuWatchdog,
    .frame = kFrame,
    .contract = kContract,
    .codeModuleArena = {},
    .padBuffers = nullptr,
};

static_assert(kLibcdInit.initialize.valid() && kCallbackBoot.initialize.valid());
static_assert(kStockLibcd.control.valid() && kStockLibcd.controlF.valid() && kStockLibcd.syncWrapper.valid() &&
              kStockLibcd.sync.valid() && kStockLibcd.read.valid() && kStockLibcd.readSync.valid());
static_assert(kGpuWatchdog.start.valid() && kGpuWatchdog.check.valid());
static_assert(kContract.guestVSync.valid());
static_assert(kContract.vsyncQueryCounter != 0, "the libetc field counter is measured, not optional");
static_assert(kFrame.coreLoop.valid() && kFrame.iteration.valid() && kFrame.transition.valid() &&
              kFrame.gpuUpdate.valid());
static_assert(kFrame.coreLoop.contains(kFrame.iteration.begin));
static_assert(kFrame.gpuUpdate.contains(kFrame.firstVSync) && kFrame.gpuUpdate.contains(kFrame.afterFirstVSync));
static_assert(kFrame.gpuUpdate.contains(kFrame.secondVSync) && kFrame.gpuUpdate.contains(kFrame.afterVSync));

} // namespace

const crash::TitleFacts &facts() {
  return kFacts;
}

const psx::host::TitleIdentity &identity() {
  return kIdentity;
}

} // namespace crash1
