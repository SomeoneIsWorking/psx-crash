#pragma once

#include "runtime/runtime.h"

#include <memory>

namespace crash2 {

// Crash 2: the shared Crash runtime over its facts, plus its draw replay.
class Runtime final : public crash::Runtime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  psx::present::ReplayFrame *replayFrame(Core &core) override;
  bool interpolates() const override {
    return true;
  }
};

} // namespace crash2
