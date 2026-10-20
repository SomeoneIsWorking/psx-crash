#pragma once

#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "runtime/title_facts.h"
#include "stock_libcd.h"
#include "stock_pad_buffers.h"
#include "stock_vsync.h"

#include <cstdio>
#include <memory>
#include <optional>

namespace crash {

// What every Crash title shares at the framework seam: crt0 and C main's boot steps, the native boot
// services, the frame turn and the `warp` command. A title supplies its measured facts and derives.
// Every title presents on Record; the canvas carries the widescreen margins.
class Runtime : public psx::game::GameRuntime {
public:
  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void deliverPad(Core &core, const psx::input::PadDelivery &delivery) override;
  void stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) override;
  bool sealedFrameIsCut(Core &core) const override;
  const psx::present::GuestWidescreenProjection *guestWidescreenProjection() const override;
  // `warp <scene-hex>`.
  bool titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) override;

  const TitleFacts &facts() const {
    return facts_;
  }

protected:
  explicit Runtime(const TitleFacts &facts);

private:
  const TitleFacts &facts_;
  psx::cd::StockLibcd libcd_;
  psx::hle::StockVSync vsync_;
  std::optional<psx::input::StockPadBuffers> pad_;
  psx::present::GuestWidescreenProjection aspectPolicy_;
};

// The facts of the title running on `core`; a core with no Crash runtime stops the run.
const TitleFacts &titleFacts(const Core &core);

} // namespace crash
