#include "crash3/render/world_renderer.h"

#include "core.h"
#include "crash3/render/world_underwater.h"
#include "native_dispatch.h"
#include "render/world_default.h"
#include "render/world_modes.h"
#include "render/world_renderer.h"

#include <array>
#include <cstdint>

namespace crash3 {
namespace {

using namespace psx::cpu;
using namespace crash::world;

// Scene modes by the flags func_80021E94 tests (default from func_80011F68).
inline constexpr std::array<Mode, 5> kModes{{
    {0x8003F518u,
     0x80041648u,
     0x800418CCu,
     0x80041768u,
     0x80041A3Cu,
     projectTriangle,
     projectQuad,
     drawTriangle,
     drawQuad},
    {0x80041E1Cu,
     0x80041C64u,
     0x80041D28u,
     0x80041CF0u,
     0x80041DD8u,
     projectSwayTriangle,
     projectSwayQuad,
     drawSwayTriangle,
     drawSwayQuad},
    {0x80043004u,
     0x800428D0u,
     0x800429E4u,
     0x80042994u,
     0x80042ADCu,
     world::projectUnderwaterTriangle,
     world::projectUnderwaterQuad,
     world::drawUnderwaterTriangle,
     world::drawUnderwaterQuad},
    {0x80041E58u,
     0x80041648u,
     0x800418CCu,
     0x80042648u,
     0x80042764u,
     projectTriangle,
     projectQuad,
     drawPulseTriangle,
     drawPulseQuad},
    {0x80041E88u,
     0x80041648u,
     0x800418C4u,
     0x80042EF8u,
     0x80042F70u,
     projectTriangle,
     projectQuadKeepFirst,
     drawGlowTriangle,
     drawGlowQuad},
}};

inline constexpr std::array<LinkTail, 3> kTails{{
    {0x80041550u, Link::Sorted},
    {0x800415A4u, Link::Bucket},
    {0x80041618u, Link::OwnSlot},
}};

constexpr Costs kCosts{
    .prologue = run(0x80040ED8u, 0x80040F58u),
    .emptyList = run(0x80040F5Cu, 0x80040F60u) + run(0x80040F64u, 0x80040F70u) + run(0x80041514u, 0x8004154Cu),
    .beforeSetup = run(0x80040F74u, 0x80040FD4u),
    .afterSetup = run(0x80040FD8u, 0x80041054u),
    // The first entry's decode is the projectors' with its load and the colour table's.
    .firstDecode = run(0x80041058u, 0x8004105Cu) + run(0x800410E4u, 0x800410E8u),
    .loopTop = run(0x800410ECu, 0x80041100u),
    .loadTriangle = run(0x80041104u, 0x8004120Cu),
    .loadQuadFirst = run(0x80041258u, 0x80041268u) + run(0x8004126Cu, 0x80041338u) + run(0x80041210u, 0x80041254u),
    .loadQuadLast = run(0x80041258u, 0x80041268u) + run(0x800413ACu, 0x800414A8u),
    .loadQuad = run(0x80041258u, 0x80041268u) + run(0x8004126Cu, 0x80041338u) + run(0x8004133Cu, 0x800413A8u),
    .nextEntry = run(0x800414ACu, 0x800414B8u),
    .linkedFinish = run(0x8004159Cu, 0x800415A0u),
    .drain = run(0x800414BCu, 0x800414C8u),
    .drainAgain = run(0x800414CCu, 0x800414D0u),
    .afterLoop = run(0x800414D4u, 0x800414ECu),
    .afterLoopUnsorted = run(0x800414D4u, 0x800414E4u),
    .linkDownHead = run(0x800414F0u, 0x800414F8u),
    .linkDownSlot = run(0x800414FCu, 0x80041508u),
    .epilogue = run(0x8004150Cu, 0x8004154Cu),
    .decodeQuad = run(0x80041650u, 0x80041690u) + run(0x800416C0u, 0x800416D0u),
    .decodeTriangle = run(0x80041650u, 0x80041668u) + run(0x80041694u, 0x800416D0u),
    .projectTriangleHead = run(0x80041648u, 0x8004164Cu),
    .projectTriangleCull = run(0x800416D4u, 0x8004170Cu),
    .projectTriangleVisible = run(0x80041718u, 0x80041758u),
    .projectTriangleTail = run(0x80041760u, 0x80041764u),
    .keepFirst = run(0x800418C4u, 0x800418C8u),
    .projectQuadHead = run(0x800418CCu, 0x800418D0u),
    .projectQuadCull = run(0x80041958u, 0x800419D0u),
    .projectQuadVisible = run(0x800419DCu, 0x80041A2Cu),
    .projectQuadTail = run(0x80041A34u, 0x80041A38u),
    .triangleHead = run(0x80041768u, 0x80041770u),
    .triangleShaded = run(0x80041774u, 0x800417A4u),
    .triangleTextured = run(0x800417A8u, 0x800417ACu),
    .triangleTexture = run(0x8004183Cu, 0x8004184Cu),
    .triangleTurn = run(0x80041854u, 0x80041874u),
    .triangleWords = run(0x80041878u, 0x80041898u),
    .triangleEnd = run(0x800418A0u, 0x800418C0u),
    .quadHead = run(0x80041A3Cu, 0x80041A44u),
    .quadShaded = run(0x80041A48u, 0x80041A78u),
    .quadTextured = run(0x80041A7Cu, 0x80041A84u),
    .quadWords = run(0x80041B14u, 0x80041B50u),
    .quadEnd = run(0x80041B58u, 0x80041B78u),
    .blended = 1,
    // The quad's drawer has its own copy of the lookup (0x80041A88..0x80041B10) with the same counts.
    .textureFixed = run(0x800417B0u, 0x800417BCu),
    .textureHead = run(0x800417C0u, 0x800417DCu),
    .textureAnimation = run(0x800417E0u, 0x8004180Cu),
    .textureFrameLevel = run(0x80041810u, 0x8004181Cu),
    .textureLevel = run(0x80041820u, 0x8004182Cu),
    .textureEnd = run(0x80041830u, 0x80041838u),
    .linkHead = run(0x80041550u, 0x80041558u),
    .relinkHead = run(0x8004155Cu, 0x80041568u),
    .relinkSlot = run(0x8004156Cu, 0x80041578u),
    .linkAppend = run(0x8004157Cu, 0x80041588u),
    .bucketHead = run(0x800415A4u, 0x800415B0u),
    .bucketCompare = run(0x800415B4u, 0x800415B8u),
    .bucketRaise = run(0x800415BCu, 0x800415BCu),
    .bucketAppend = run(0x800415C0u, 0x800415D0u),
    .bucketFront = run(0x800415ECu, 0x800415FCu),
    .ownSlot = run(0x80041618u, 0x8004162Cu),
};

inline constexpr Title kTitle{
    .owner = "Crash 3 world renderer",
    .drawWorlds = 0x80040ED8u,
    .resetOt = 0x8003AC44u,
    .resetOtReturn = 0x80040F64u,
    .setupReturn = 0x80040FD8u,
    .depthShiftTable = 0x8005D350u,
    .frameOt = 0x20u,
    .sceneModes = 0x14u,
    .sceneLinkTail = 0x28u,
    .sceneLevelShift = 0x30u,
    .vertexShift = 2u,
    .averageDepth = true,
    .averageScale3 = 0x555u,
    .averageScale4 = 0x400u,
    .storesEntryWorld = true,
    .modes = kModes,
    .tails = kTails,
    .costs = kCosts,
    .sway = {.sineTable = 0x8005C91Cu, .angleShift = 2u, .thirdFromGte = true},
    .pulse = {.pulseBit = kBit18, .dimBit = kBit19, .dimNear = 0x96u, .span = 0xB6u},
    .glow = {.colourWord = scratch::kCornerColour,
             .firstReach = 0x834u,
             .laterReach = 0x514u,
             .lightShift = 2u,
             .cornerHead = run(0x80042D2Cu, 0x80042D38u),
             .flatTriangle = run(0x80042E40u, 0x80042E70u),
             .flatQuad = run(0x80042EA8u, 0x80042ED8u)},
};

void draw(Core *core) {
  drawWorlds(*core, kTitle);
}

} // namespace

void WorldRenderer::registerOverrides(Core &core) {
  installNativeOverride(core, kTitle.drawWorlds, kTitle.owner, draw, NativeReturn::Void);
}

} // namespace crash3
