#pragma once

#include "runtime/runtime.h"

#include <memory>

namespace crash1 {

// Crash 1: the shared Crash runtime over its facts, plus its block pool, BIOS pad word and draw replay.
class Runtime final : public crash::Runtime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  psx::present::ReplayFrame *replayFrame(Core &core) override;
  void reportRun(Core &core) const override;
  bool interpolates() const override {
    return true;
  }
};

} // namespace crash1
