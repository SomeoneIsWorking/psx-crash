#include "crash2/render/world_renderer.h"

#include "core.h"
#include "crash2/render/world_steps.h"
#include "native_dispatch.h"
#include "render/world_renderer.h"

#include <array>
#include <cstdint>

namespace crash2 {
namespace {

using namespace psx::cpu;
using crash::world::Costs;
using crash::world::Mode;
using crash::world::run;

inline constexpr std::array<Mode, 10> kModes{{
    {0x8003DD0Cu,
     0x80042420u,
     0x800426A0u,
     0x80042548u,
     0x80042818u,
     crash::world::projectTriangle,
     crash::world::projectQuad,
     crash::world::drawTriangle,
     crash::world::drawQuad},
    {0x8004599Cu,
     0x80042420u,
     0x800426A0u,
     0x800459A8u,
     0x80045AB4u,
     crash::world::projectTriangle,
     crash::world::projectQuad,
     world::drawFogTriangle,
     world::drawFogQuad},
    {0x8004399Cu,
     0x80042420u,
     0x800426A0u,
     0x800439D4u,
     0x80043A84u,
     crash::world::projectTriangle,
     crash::world::projectQuad,
     world::drawDepthFogTriangle,
     world::drawDepthFogQuad},
    {0x80044980u,
     0x80042420u,
     0x800426A0u,
     0x800449B0u,
     0x80044A04u,
     crash::world::projectTriangle,
     crash::world::projectQuad,
     world::drawTintTriangle,
     world::drawTintQuad},
    {0x80043D54u,
     0x80042420u,
     0x800426A0u,
     0x80043D84u,
     0x80043EA0u,
     crash::world::projectTriangle,
     crash::world::projectQuad,
     world::drawPulseTriangle,
     world::drawPulseQuad},
    {0x80042C70u,
     0x80042420u,
     0x80042698u,
     0x80042E84u,
     0x80042F08u,
     crash::world::projectTriangle,
     crash::world::projectQuadKeepFirst,
     world::drawLampTriangle,
     world::drawLampQuad},
    {0x80044A70u,
     0x80042420u,
     0x80042698u,
     0x80044E48u,
     0x80044EC0u,
     crash::world::projectTriangle,
     crash::world::projectQuadKeepFirst,
     world::drawGlowTriangle,
     world::drawGlowQuad},
    {0x80043B5Cu,
     0x80043B74u,
     0x80043C48u,
     0x80043C2Cu,
     0x80043D34u,
     world::projectWaveTriangle,
     world::projectWaveQuad,
     world::drawWaveTriangle,
     world::drawWaveQuad},
    {0x8004299Cu,
     0x80042AB8u,
     0x80042B7Cu,
     0x80042B44u,
     0x80042C2Cu,
     world::projectSwayTriangle,
     world::projectSwayQuad,
     world::drawSwayTriangle,
     world::drawSwayQuad},
    {0x80045C18u,
     0x80045D88u,
     0x80045F40u,
     0x80042548u,
     0x80042818u,
     world::projectLightTriangle,
     world::projectLightQuad,
     crash::world::drawTriangle,
     crash::world::drawQuad},
}};

constexpr Costs kCosts{
    .prologue = run(0x80041E5Cu, 0x80041ECCu),
    .emptyList = run(0x80041ED0u, 0x80041ED4u) + run(0x80041ED8u, 0x80041EE4u) + run(0x800423E4u, 0x8004241Cu),
    .beforeSetup = run(0x80041EE8u, 0x80041F38u),
    .afterSetup = run(0x80041F3Cu, 0x80041FD8u),
    .firstDecode = run(0x80042030u, 0x80042048u),
    .loopTop = run(0x8004204Cu, 0x80042060u),
    .loadTriangle = run(0x80042064u, 0x80042148u),
    .loadQuadFirst = run(0x80042190u, 0x800421A0u) + run(0x800421A4u, 0x80042250u) + run(0x8004214Cu, 0x8004218Cu),
    .loadQuadLast = run(0x80042190u, 0x800421A0u) + run(0x800422B4u, 0x8004238Cu),
    .loadQuad = run(0x80042190u, 0x800421A0u) + run(0x800421A4u, 0x80042250u) + run(0x80042254u, 0x800422B0u),
    .nextEntry = run(0x80042390u, 0x8004239Cu),
    .linkedFinish = run(0x80042994u, 0x80042998u),
    .drain = run(0x800423A0u, 0x800423ACu),
    .drainAgain = run(0x800423B0u, 0x800423B4u),
    .afterLoop = run(0x800423B8u, 0x800423BCu),
    .linkDownHead = run(0x800423C0u, 0x800423C8u),
    .linkDownSlot = run(0x800423CCu, 0x800423D8u),
    .epilogue = run(0x800423DCu, 0x8004241Cu),
    .decodeQuad = run(0x80042440u, 0x80042464u),
    .decodeTriangle = run(0x80042468u, 0x80042490u),
    .projectTriangleHead = run(0x80042420u, 0x8004243Cu),
    .projectTriangleCull = run(0x80042494u, 0x800424DCu),
    .projectTriangleVisible = run(0x800424E8u, 0x80042538u),
    .projectTriangleTail = run(0x80042540u, 0x80042544u),
    .keepFirst = run(0x80042698u, 0x8004269Cu),
    .projectQuadHead = run(0x800426A0u, 0x800426BCu),
    .projectQuadCull = run(0x80042714u, 0x8004279Cu),
    .projectQuadVisible = run(0x800427A8u, 0x80042808u),
    .projectQuadTail = run(0x80042810u, 0x80042814u),
    .triangleHead = run(0x80042548u, 0x80042550u),
    .triangleShaded = run(0x80042554u, 0x80042580u),
    .triangleTextured = run(0x80042584u, 0x80042588u),
    .triangleTexture = run(0x80042614u, 0x80042628u),
    .triangleTurn = run(0x8004262Cu, 0x8004264Cu),
    .triangleWords = run(0x80042650u, 0x80042670u),
    .triangleEnd = run(0x80042678u, 0x80042694u),
    .quadHead = run(0x80042818u, 0x80042820u),
    .quadShaded = run(0x80042824u, 0x80042850u),
    .quadTextured = run(0x80042854u, 0x8004285Cu),
    .quadWords = run(0x800428E8u, 0x80042924u),
    .quadEnd = run(0x8004292Cu, 0x80042944u),
    .blended = 1,
    .textureFixed = run(0x8004258Cu, 0x80042598u),
    .textureHead = run(0x8004259Cu, 0x800425B4u),
    .textureAnimation = run(0x800425B8u, 0x800425E4u),
    .textureFrameLevel = run(0x800425E8u, 0x800425F4u),
    .textureLevel = run(0x800425F8u, 0x80042604u),
    .textureEnd = run(0x80042608u, 0x80042610u),
    .linkHead = run(0x80042948u, 0x80042950u),
    .relinkHead = run(0x80042954u, 0x80042960u),
    .relinkSlot = run(0x80042964u, 0x80042970u),
    .linkAppend = run(0x80042974u, 0x80042980u),
};

inline constexpr crash::world::Title kTitle{
    .owner = "Crash 2 Gfx_DrawWorlds",
    .drawWorlds = 0x80041E5Cu,
    .resetOt = 0x8003BC5Cu,
    .resetOtReturn = 0x80041ED8u,
    .setupReturn = 0x80041F3Cu,
    .depthShiftTable = 0x8005C724u,
    .frameOt = 0x18u,
    .sceneModes = 0x10u,
    .modes = kModes,
    .costs = kCosts,
};

void draw(Core *core) {
  crash::world::drawWorlds(*core, kTitle);
}

} // namespace

void WorldRenderer::registerOverrides(Core &core) {
  installNativeOverride(core, kTitle.drawWorlds, kTitle.owner, draw, NativeReturn::Void);
}

} // namespace crash2
