#include "crash3/entry/facts.h"

#ifndef CRASH_STATIC_CONSTRUCTORS_ENTRY
#error "Crash 3 native facts must come from titles/crash3/executable.json"
#endif

namespace crash3 {
namespace {

constexpr psx::game::Crt0Image kCrt0{.entry = 0x800489F8u};

// CD_init 0x80047188 is word-for-word Crash 2's: callbacks, status, last command, sync bytes, no workspace.
constexpr crash::libcd_init::Program kLibcdInit{
    .initialize = {CRASH_CD_INITIALIZE_ENTRY, CRASH_CD_INITIALIZE_END},
    .lastSyncCallback = 0x8005DE4Cu,
    .lastReadyCallback = 0x8005DE50u,
    .lastStatus = 0x8005DE5Cu,
    .lastResult = 0x8005DE60u,
    .lastCommand = 0x8005DE6Cu,
    .pendingCommand = 0x8005DE6Du,
    .commandWorkspace = 0u,
    .commandWorkspaceWords = 0u,
    .syncStatus = 0x8005E12Cu,
};

constexpr psx::cd::StockLibcd::Entries kStockLibcd{
    .read = CRASH_CD_READ_ENTRY,
    .readSync = CRASH_CD_READ_SYNC_ENTRY,
    .control = CRASH_CD_CONTROL_ENTRY,
    .controlFire = CRASH_CD_CONTROL_F_ENTRY,
    .sync = CRASH_CD_SYNC_ENTRY,
};

// FUN_80017AE8 has Crash 2's initializer layout: the same BIOS leaves and gp-relative handle slots.
constexpr crash::callback_boot::Program kCallbackBoot{
    .initialize = {CRASH_CALLBACK_INITIALIZE_ENTRY, CRASH_CALLBACK_INITIALIZE_END},
    .enterCritical = 0x80048C38u,
    .openEvent = 0x80048BE8u,
    .exitCritical = 0x80048C48u,
    .closeEvent = 0x80048C18u,
    .initializePad = 0x8005B6A0u,
    .startPad = 0x8005B6F8u,
    .changeClearPad = 0x80048B98u,
    .handleOffsets = {0x1F0u, 0x1F4u, 0x1F8u, 0x1FCu, 0x200u, 0x204u, 0x208u, 0x20Cu},
};

// GpuUpdate 0x80016A8C: `gp+0x70 - displayed->0x1C < 2` guards both waits; the VSync callback 0x80017AD0
// bumps gp+0x70, and a frame after a load (0x80061054 non-zero) keeps the displayed record's stale stamp.
constexpr crash::FrameLimiter kLimiter{
    .vblankCounter = 0x800608E8u,
    .displayedFrame = 0x80061A7Cu,
    .stampOffset = 0x1Cu,
    .skipAfterFields = 2u,
};

// The level loader's wait FUN_80012EAC: page-load service FUN_80012CBC(-1) returning to 0x80012F08, then
// GpuUpdate(1) once gp+0x70 differs from displayed->0x1C; 0x800691FC counts loading frames, -1 when done.
constexpr crash::LoaderPoll kLoaderPoll{
    .service = 0x80012CBCu,
    .afterService = 0x80012F08u,
    .loadingFrames = 0x800691FCu,
};

// The scene table at 0x8006C4D8 has Crash 2's row layout.
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
    .limiter = kLimiter,
    .loadingSwap = std::nullopt,
    .loaderPoll = kLoaderPoll,
    .doneAddress = CRASH_DONE_ADDRESS,
    .sceneIdAddress = CRASH_SCENE_ID_ADDRESS,
    .sceneRequestAddress = CRASH_SCENE_REQUEST_ADDRESS,
    .sceneTable = {0x8006C4D8u, 0x3Du, 12u, 0x1FFFFu},
    .spawns = std::nullopt,
    .rootCounterIncrement = CRASH_ROOT_COUNTER_INCREMENT,
    .setRootCounter = CRASH_SET_ROOT_COUNTER,
    .startRootCounter = CRASH_START_ROOT_COUNTER,
    .stopRootCounter = CRASH_STOP_ROOT_COUNTER,
};

constexpr psx::host::TitleIdentity kIdentity{
    .displayName = CRASH_EXE_TITLE,
    .serial = CRASH_EXE_NAME,
    .slug = "crash3",
    .fileSize = CRASH_EXE_EXPECTED_SIZE,
    .sha256 = CRASH_EXE_SHA256,
    .entry = CRASH_EXE_ENTRY,
    .globalPointer = CRASH_EXE_GP,
    .textAddress = CRASH_EXE_TEXT_ADDRESS,
    .textSize = CRASH_EXE_TEXT_SIZE,
    .stackAddress = CRASH_EXE_STACK_ADDRESS,
    .stackOffset = CRASH_EXE_STACK_OFFSET,
    .discEnvVar = "PSXPORT_CRASH3_DISC",
    .discExecutablePath = CRASH_EXE_NAME,
};

// The draw replay, read from the retail executable's disassembly (crash3-decomp docs/notes/frame-loop.md): the same
// engine as Crash 2's, GpuUpdate as its end of frame, gp 0x80060878.
constexpr crash::ReplayFacts kReplay{
    .mainLoop = 0x8001166Cu,
    .viewCall = 0x80011A4Cu,
    .objectsStop = 0x8001D16Cu,
    .drawObjects = 0x80011D98u,
    .endFrameStop = 0x80016634u,
    .sampleStub = 0x80011FC0u,
    .objUpdateAll = 0x8001D16Cu,
    .resetFrame = 0x8001660Cu,
    .endFrame = 0x80016634u,
    .endFrameArgument = 0u,
    .endFrameDeviceTail = 0x800169ECu,
    .buildBuffer = 0x80061A84u,
    .orderingTable = 0x20u,
    .packetHead = 0x2090u,
    .paused = 0x8006087Cu,
    .frameFlags = 0x80068F1Cu,
    .objectLogic = 0x8u,
    .additiveFade = 0x800000u,
    .objectsFirst = 0x40000000u,
    .updateView = 0x80018A54u,
    .screenShake = 0x80068F00u,
    .streakScroll = std::nullopt,
    .drawFade = 0x800176A0u,
    .objForEachList = 0x80019A38u,
    .objUpdate = 0x8001D674u,
    .queueObject = 0x8001B7A0u,
    .tracks =
        {.camera = 0x80065CF0u,
         .objectPool = 0x80068E9Cu,
         .objectStride = 0x23Cu,
         .objectCount = 0x60u,
         .player = 0x80060A90u,
         .object = {.exec = 0x0Cu, .trans = 0x60u, .rot = 0x6Cu, .scale = 0x78u, .anim = 0xE8u, .animFrame = 0xECu}},
};

// The vertex poses and the unlit morph reader (crash3-decomp docs/notes/frame-loop.md, Animation): the pose fetch
// 0x8001B0BC, the reader 0x8003E0FC (Crash 2's, its sum scaled 8x), the stream opener 0x8003F63C and the morph draw
// steps (table 0x8005C270's second entries).
constexpr crash::PoseFacts kPoses{
    .fetchPose = 0x8001B0BCu,
    .block = 0x08u,
    .scale = 0x78u,
    .anim = 0xE8u,
    .animFrame = 0xECu,
    .firstFrame = 0x20u,
    .secondFrame = 0x24u,
    .vertexKind = 1,
    .frameModel = 0x0Cu,
    .reader = {.owner = "Crash 3 morph reader",
               .unlitReader = 0x8003E0FCu,
               .openStreams = 0x8003F63Cu,
               .frameData = 0x18u,
               .packetFirstFrame = 0x30u,
               .packetSecondFrame = 0x34u,
               .steps = {0x8003D36Cu, 0x8003D748u, 0x8003D788u, 0x8003D80Cu, 0x8003D8F8u, 0x8003E39Cu, 0x8003F2FCu},
               .stepCount = 7,
               .sumXyShift = 3,
               .sumZShift = 5},
};

// The GPU watchdog check differs from Crash 1 and 2's, so it runs as guest code. libpad PadInitDirect
// reads the pad through the title's own SIO code, so no receive buffers are published.
constexpr crash::TitleFacts kFacts{
    .crt0 = kCrt0,
    .staticConstructors = CRASH_STATIC_CONSTRUCTORS_ENTRY,
    .useCdAddress = CRASH_USE_CD_ADDRESS,
    .init = CRASH_INIT_ENTRY,
    .libcdInit = kLibcdInit,
    .stockLibcd = kStockLibcd,
    .callbackBoot = kCallbackBoot,
    .gpuWatchdog = std::nullopt,
    .frame = kFrame,
    .vsync = {CRASH_TITLE_VSYNC_ENTRY, CRASH_TITLE_VSYNC_END},
    .vsyncQueryCounter = CRASH_TITLE_VSYNC_QUERY_COUNTER,
    .codeModuleArena = {0x0006EA78u, 0x00200000u},
    .padBuffers = std::nullopt,
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

} // namespace crash3
