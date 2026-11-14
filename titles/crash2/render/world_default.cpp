// The default scene mode's projectors and drawers, which every other mode ends in.
#include "crash2/render/world_steps.h"

#include "gte_registers.h"

namespace crash2::world {
namespace {

using psx::gte::kRtps;
using psx::gte::kRtpt;

// GPU primitive codes in the colour word and the texture page's blending bits (21, 22; both set: opaque).
inline constexpr std::uint32_t kPolyG3 = 0x30000000u;
inline constexpr std::uint32_t kPolyGt3 = 0x34000000u;
inline constexpr std::uint32_t kPolyGt3Blended = 0x36000000u;
inline constexpr std::uint32_t kPolyG4 = 0x38000000u;
inline constexpr std::uint32_t kPolyGt4 = 0x3C000000u;
inline constexpr std::uint32_t kPolyGt4Blended = 0x3E000000u;
inline constexpr std::uint32_t kPolyF3 = 0x20000000u;
inline constexpr std::uint32_t kPolyF4 = 0x28000000u;
inline constexpr std::uint32_t kTextureBlend = 0x600000u;
inline constexpr std::uint32_t kMaterialBits = 20u;

bool untextured(const Pass &pass) {
  return (pass.material << kMaterialBits) == 0u;
}

bool opaque(std::uint32_t textureSecond) {
  return (textureSecond & kTextureBlend) == kTextureBlend;
}

} // namespace

void projectTriangle(Pass &pass) {
  pass.charge(run(0x80042420u, 0x8004243Cu));
  pass.command(kRtpt);
  pass.colour[0] = pass.vertexColour(0);
  pass.decodeNext();
  pass.charge(run(0x80042494u, 0x800424DCu));
  const std::uint32_t screens[3] = {pass.screen(0), pass.screen(1), pass.screen(2)};
  const Cull cull = pass.offScreen(screens, 3);
  if (cull == Cull::Rows) {
    return;
  }
  if (cull == Cull::Columns) {
    return;
  }
  pass.colour[1] = pass.vertexColour(1);
  pass.colour[2] = pass.vertexColour(2);
  pass.charge(run(0x800424E8u, 0x80042538u) + pass.depthSlot() + run(0x80042540u, 0x80042544u));
  pass.drawTriangleStep(pass);
}

void projectQuadKeepFirst(Pass &pass) {
  pass.charge(run(0x80042698u, 0x8004269Cu));
  pass.storeScratch(scratch::kQuadFirstVxy, pass.gteData(psx::gte::kVxy0));
  pass.storeScratch(scratch::kQuadFirstVz, pass.gteData(psx::gte::kVz0));
  projectQuad(pass);
}

void projectQuad(Pass &pass) {
  pass.charge(run(0x800426A0u, 0x800426BCu));
  pass.command(kRtpt);
  pass.colour[0] = pass.vertexColour(0);
  pass.decodeNext();
  pass.storeScreen(scratch::kBase + scratch::kQuadFirstXy, 0);
  pass.storeScratch(scratch::kQuadFirstZ, pass.gteData(psx::gte::kSz1));
  pass.setGteData(psx::gte::kVxy0, pass.lastXy);
  pass.setGteData(psx::gte::kVz0, pass.lastZ);
  std::uint32_t screens[4] = {pass.screen(0), pass.screen(1), pass.screen(2), 0};
  pass.storeScreen(pass.next + 8u, 0);
  pass.command(kRtps);
  pass.colour[1] = pass.vertexColour(1);
  screens[3] = pass.screen(2);
  pass.charge(run(0x80042714u, 0x8004279Cu));
  const Cull cull = pass.offScreen(screens, 4);
  if (cull == Cull::Rows) {
    return;
  }
  if (cull == Cull::Columns) {
    return;
  }
  pass.colour[2] = pass.vertexColour(2);
  pass.colour[3] = pass.vertexColour(3);
  pass.charge(run(0x800427A8u, 0x80042808u) + pass.depthSlot() + run(0x80042810u, 0x80042814u));
  pass.drawQuadStep(pass);
}

void drawTriangle(Pass &pass) {
  const std::uint32_t primitive = pass.next;
  pass.charge(run(0x80042548u, 0x80042550u));
  if (untextured(pass)) {
    pass.charge(run(0x80042554u, 0x80042580u));
    pass.storeScreen(primitive + 0x08u, 0);
    pass.storeScreen(primitive + 0x10u, 1);
    pass.storeScreen(primitive + 0x18u, 2);
    pass.store(primitive + 0x04u, pass.colour[0] | kPolyG3);
    pass.store(primitive + 0x0Cu, pass.colour[1]);
    pass.store(primitive + 0x14u, pass.colour[2]);
    pass.linkPrimitive(7);
    return;
  }
  pass.charge(run(0x80042584u, 0x80042588u));
  const std::array<std::uint32_t, 3> texture = pass.triangleTexture();
  pass.charge(run(0x80042614u, 0x80042628u));
  if (pass.scratchWord(scratch::kRotated) != 0u) {
    pass.charge(run(0x8004262Cu, 0x8004264Cu));
  }
  pass.store(primitive + 0x0Cu, texture[0]);
  pass.store(primitive + 0x18u, texture[1]);
  pass.store(primitive + 0x24u, texture[2]);
  pass.store(primitive + 0x10u, pass.colour[1]);
  pass.store(primitive + 0x1Cu, pass.colour[2]);
  pass.charge(run(0x80042650u, 0x80042670u));
  std::uint32_t code = kPolyGt3;
  if (!opaque(texture[1])) {
    pass.charge(1);
    code = kPolyGt3Blended;
  }
  pass.charge(run(0x80042678u, 0x80042694u));
  pass.store(primitive + 0x04u, pass.colour[0] | code);
  pass.storeScreen(primitive + 0x08u, 0);
  pass.storeScreen(primitive + 0x14u, 1);
  pass.storeScreen(primitive + 0x20u, 2);
  pass.linkPrimitive(10);
}

void drawQuad(Pass &pass) {
  const std::uint32_t primitive = pass.next;
  pass.charge(run(0x80042818u, 0x80042820u));
  if (untextured(pass)) {
    pass.charge(run(0x80042824u, 0x80042850u));
    pass.storeScreen(primitive + 0x10u, 0);
    pass.storeScreen(primitive + 0x18u, 1);
    pass.storeScreen(primitive + 0x20u, 2);
    pass.store(primitive + 0x04u, pass.colour[0] | kPolyG4);
    pass.store(primitive + 0x0Cu, pass.colour[1]);
    pass.store(primitive + 0x14u, pass.colour[2]);
    pass.store(primitive + 0x1Cu, pass.colour[3]);
    pass.linkPrimitive(9);
    return;
  }
  pass.charge(run(0x80042854u, 0x8004285Cu));
  const std::uint32_t texture = pass.textureAddress();
  const std::uint32_t first = pass.word(texture);
  const std::uint32_t second = pass.word(texture + 4u);
  const std::uint32_t third = pass.word(texture + 8u);
  pass.charge(run(0x800428E8u, 0x80042924u));
  pass.store(primitive + 0x0Cu, first);
  pass.store(primitive + 0x18u, second);
  pass.store(primitive + 0x24u, third);
  pass.store(primitive + 0x30u, third >> 16);
  pass.store(primitive + 0x10u, pass.colour[1]);
  pass.store(primitive + 0x1Cu, pass.colour[2]);
  pass.store(primitive + 0x28u, pass.colour[3]);
  std::uint32_t code = kPolyGt4;
  if (!opaque(second)) {
    pass.charge(1);
    code = kPolyGt4Blended;
  }
  pass.charge(run(0x8004292Cu, 0x80042944u));
  pass.store(primitive + 0x04u, pass.colour[0] | code);
  pass.storeScreen(primitive + 0x14u, 0);
  pass.storeScreen(primitive + 0x20u, 1);
  pass.storeScreen(primitive + 0x2Cu, 2);
  pass.linkPrimitive(13);
}

void drawFlatTriangle(Pass &pass) {
  const std::uint32_t primitive = pass.next;
  pass.storeScreen(primitive + 0x08u, 0);
  pass.storeScreen(primitive + 0x0Cu, 1);
  pass.storeScreen(primitive + 0x10u, 2);
  pass.store(primitive + 0x04u, kPolyF3);
  pass.linkPrimitive(5);
}

void drawFlatQuad(Pass &pass) {
  const std::uint32_t primitive = pass.next;
  pass.storeScreen(primitive + 0x0Cu, 0);
  pass.storeScreen(primitive + 0x10u, 1);
  pass.storeScreen(primitive + 0x14u, 2);
  pass.store(primitive + 0x04u, kPolyF4);
  pass.linkPrimitive(6);
}

} // namespace crash2::world
