#include "crash2/entry/facts.h"

#ifndef CRASH_STATIC_CONSTRUCTORS_ENTRY
#error "Crash 2 native facts must come from titles/crash2/executable.json"
#endif

namespace crash2 {
namespace {

constexpr psx::game::Crt0Image kCrt0{.entry = 0x80049B2Cu};

// CD_init 0x800482BC (decompiled) resets the callbacks, status, last command and sync bytes; this libcd
// has no command workspace.
constexpr crash::libcd_init::Program kLibcdInit{
    .initialize = {CRASH_CD_INITIALIZE_ENTRY, CRASH_CD_INITIALIZE_END},
    .lastSyncCallback = 0x8005C7CCu,
    .lastReadyCallback = 0x8005C7D0u,
    .lastStatus = 0x8005C7DCu,
    .lastResult = 0x8005C7E0u,
    .lastCommand = 0x8005C7ECu,
    .pendingCommand = 0x8005C7EDu,
    .commandWorkspace = 0u,
    .commandWorkspaceWords = 0u,
    .syncStatus = 0x8005CAACu,
};

constexpr psx::cd::StockLibcd::Entries kStockLibcd{
    .read = CRASH_CD_READ_ENTRY,
    .readSync = CRASH_CD_READ_SYNC_ENTRY,
    .control = CRASH_CD_CONTROL_ENTRY,
    .controlFire = CRASH_CD_CONTROL_F_ENTRY,
    .sync = CRASH_CD_SYNC_ENTRY,
};

// FUN_80036164 calls the same BIOS leaves in the same order as Crash 1's initializer; its handles land
// in gp-relative slots 0x224..0x250.
constexpr crash::callback_boot::Program kCallbackBoot{
    .initialize = {CRASH_CALLBACK_INITIALIZE_ENTRY, CRASH_CALLBACK_INITIALIZE_END},
    .enterCritical = 0x80049D1Cu,
    .openEvent = 0x80049CCCu,
    .exitCritical = 0x80049D2Cu,
    .closeEvent = 0x80049CFCu,
    .initializePad = 0x8005B030u,
    .startPad = 0x8005B088u,
    .changeClearPad = 0x80049CACu,
    .handleOffsets = {0x224u, 0x228u, 0x22Cu, 0x230u, 0x240u, 0x244u, 0x24Cu, 0x250u},
};

constexpr crash::gpu_watchdog::Program kGpuWatchdog{
    .start = {CRASH_GPU_WATCHDOG_START_ENTRY, CRASH_GPU_WATCHDOG_START_END},
    .check = {CRASH_GPU_WATCHDOG_CHECK_ENTRY, CRASH_GPU_WATCHDOG_CHECK_END},
    .deadline = 0x8005DE34u,
    .pollCount = 0x8005DE38u,
    .queueRead = 0x8005DE24u,
    .queueWrite = 0x8005DE20u,
    .gp0Pointer = 0x8005DDF0u,
    .statusPointer = 0x8005DDF4u,
    .gp1Pointer = 0x8005DDFCu,
    .dmaControlPointer = 0x8005DE0Cu,
    .lastCallback = 0x8005DE10u,
    .lastPayload = 0x8005DE14u,
    .lastArgument = 0x8005DE18u,
    .criticalToken = 0x8005DE30u,
    .diagnosticFormat = 0x80011040u,
    .callbackFormat = 0x80011074u,
    .log = 0x80049244u,
    .critical = 0x8004A7F4u,
};

// The scene table is FUN_80014C68 and FUN_800123F0's row source: sector, byte size, end sector per id.
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
    .limiter = std::nullopt,
    .loadingSwap = std::nullopt,
    .loaderPoll = std::nullopt,
    .doneAddress = CRASH_DONE_ADDRESS,
    .sceneIdAddress = CRASH_SCENE_ID_ADDRESS,
    .sceneRequestAddress = CRASH_SCENE_REQUEST_ADDRESS,
    .sceneTable = {0x80069034u, 0x3Du, 12u, 0x1FFFFu},
    // g_LevelHeader (NsdLevel), the spawn index func_8001F9C4 starts the level at, its warp-room level and the
    // indices the C and GOOL code store for other levels (crash2-decomp docs/notes/spawn-index.md).
    .spawns = crash::SpawnPoints{.levelHeader = 0x8006759Cu,
                                 .spawnIndex = 0x8006CD7Cu,
                                 .levelIdOffset = 8u,
                                 .warpRoomLevel = 2u,
                                 .carriedSpawns = 2u},
    .rootCounterIncrement = CRASH_ROOT_COUNTER_INCREMENT,
    .setRootCounter = CRASH_SET_ROOT_COUNTER,
    .startRootCounter = CRASH_START_ROOT_COUNTER,
    .stopRootCounter = CRASH_STOP_ROOT_COUNTER,
};

// libpad InitPAD receive buffers, one per port.
constexpr psx::input::StockPadBuffers::Layout kPadBuffers{.slot0Buffer = 0x80069920u, .slot1Buffer = 0x8006996Cu};

constexpr psx::host::TitleIdentity kIdentity{
    .displayName = CRASH_EXE_TITLE,
    .serial = CRASH_EXE_NAME,
    .slug = "crash2",
    .fileSize = CRASH_EXE_EXPECTED_SIZE,
    .sha256 = CRASH_EXE_SHA256,
    .entry = CRASH_EXE_ENTRY,
    .globalPointer = CRASH_EXE_GP,
    .textAddress = CRASH_EXE_TEXT_ADDRESS,
    .textSize = CRASH_EXE_TEXT_SIZE,
    .stackAddress = CRASH_EXE_STACK_ADDRESS,
    .stackOffset = CRASH_EXE_STACK_OFFSET,
    .discEnvVar = "PSXPORT_CRASH2_DISC",
    .discExecutablePath = CRASH_EXE_NAME,
};

// The draw replay: names and sites from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr crash::ReplayFacts kReplay{
    .mainLoop = 0x80011800u,
    .viewCall = 0x80011B68u,
    .objectsStop = 0x80011D80u,
    .drawObjects = 0x80011D94u,
    .endFrameStop = 0x80011DB4u,
    .sampleStub = 0x80011E88u,
    .objUpdateAll = 0x8001C29Cu,
    .resetFrame = 0x800163C4u,
    .endFrame = 0x8001645Cu,
    .endFrameArgument = std::nullopt,
    .endFrameDeviceTail = 0x80016790u,
    .buildBuffer = 0x80063458u,
    .orderingTable = 0x18u,
    .packetHead = 0x2088u,
    .paused = 0x8005F180u,
    .frameFlags = 0x8006CB94u,
    .objectLogic = 0x8u,
    .additiveFade = 0x800000u,
    .objectsFirst = 0u,
    .updateView = 0x80017BC4u,
    .screenShake = 0x8006CB78u,
    .streakScroll = 0x80062A6Cu,
    .drawFade = 0x80016B08u,
    .objForEachList = 0x80018DD0u,
    .objUpdate = 0x8001C718u,
    .queueObject = 0x8001ABF8u,
    .tracks =
        {.camera = 0x800607F4u,
         .objectPool = 0x8006CB14u,
         .objectStride = 0x23Cu,
         .objectCount = 0x60u,
         .player = 0x8005F38Cu,
         .object = {.exec = 0x0Cu, .trans = 0x60u, .rot = 0x6Cu, .scale = 0x78u, .anim = 0xE8u, .animFrame = 0xECu}},
};

// The vertex poses and the unlit morph reader (crash2-decomp src/game/8001A014.c, docs/notes/frame-loop.md, Animation):
// the pose fetch func_8001A4A4, the reader func_8003ED14, the stream opener 0x80043658 and the morph draw steps
// (table 0x8005B708) that reach a reader.
constexpr crash::PoseFacts kPoses{
    .fetchPose = 0x8001A4A4u,
    .block = 0x08u,
    .scale = 0x78u,
    .anim = 0xE8u,
    .animFrame = 0xECu,
    .firstFrame = 0x20u,
    .secondFrame = 0x24u,
    .vertexKind = 1,
    .frameModel = 0x10u,
    .reader = {.owner = "Crash 2 morph reader",
               .unlitReader = 0x8003ED14u,
               .openStreams = 0x80043658u,
               .frameData = 0x14u,
               .packetFirstFrame = 0x28u,
               .packetSecondFrame = 0x2Cu,
               .steps = {0x8003F1C8u, 0x8003F1D8u, 0x8003F410u, 0x80040FC4u, 0x800417B0u},
               .stepCount = 5,
               .sumXyShift = 0,
               .sumZShift = 8},
};

constexpr crash::TitleFacts kFacts{
    .crt0 = kCrt0,
    .staticConstructors = CRASH_STATIC_CONSTRUCTORS_ENTRY,
    .useCdAddress = CRASH_USE_CD_ADDRESS,
    .init = CRASH_INIT_ENTRY,
    .libcdInit = kLibcdInit,
    .stockLibcd = kStockLibcd,
    .callbackBoot = kCallbackBoot,
    .gpuWatchdog = kGpuWatchdog,
    .frame = kFrame,
    .vsync = {CRASH_TITLE_VSYNC_ENTRY, CRASH_TITLE_VSYNC_END},
    .vsyncQueryCounter = CRASH_TITLE_VSYNC_QUERY_COUNTER,
    .codeModuleArena = {0x0006F1F0u, 0x00200000u},
    .padBuffers = kPadBuffers,
    .replay = kReplay,
    .poses = kPoses,
};

static_assert(crash::isCoherent(kFacts));

} // namespace

const crash::TitleFacts &facts() {
  return kFacts;
}

const psx::host::TitleIdentity &identity() {
  return kIdentity;
}

} // namespace crash2
