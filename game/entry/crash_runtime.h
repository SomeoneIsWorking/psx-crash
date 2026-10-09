#pragma once

#include "frame_cut.h"
#include "game_runtime.h"
#include "guest_projection_publication.h"
#include "native_frame_loop_contract.h"
#include "platform_hle.h"
#include "title_facts.h"

#include <memory>

namespace crash {

// What every Crash title shares at the framework seam: the boot sequence, the native boot services, the
// frame turn and the widescreen owner. A title supplies its measured facts and its own native owners.
class CrashRuntime : public GameRuntime {
public:
  RenderCapabilities renderCapabilities() const override;
  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  const GuestPadBufferLayout *guestPadBufferLayout() const override;
  bool guestVramIsPicture(const Game &game) const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  bool sealedFrameIsCut(Core &core) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  void stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) override;
  bool controlCommand(Core &core, const char *cmd, const char *line, FILE *out) override;
  const char *discEnvVar() const override;
  void reportRun(Core &core) const override;

  const TitleFacts &facts() const {
    return facts_;
  }
  const NativeFrameLoopContract &nativeFrameLoopContract() const {
    return facts_.contract;
  }

protected:
  CrashRuntime(const TitleFacts &facts, GuestProjectionPublication &projection, std::string_view logDomain);

  // The title's own native owners, installed after the shared ones and before the frame driver's.
  virtual void registerTitleOverrides(Game &game) = 0;

  FrameCut &frameCut() {
    return frameCut_;
  }

private:
  const TitleFacts &facts_;
  GuestProjectionPublication &projection_;
  std::string_view logDomain_;
  PlatformHlePlan platformPlan_;
  FrameCut frameCut_;
};

} // namespace crash
