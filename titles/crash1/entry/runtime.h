#pragma once

#include "runtime/runtime.h"

#include <memory>

namespace crash1 {

// Crash 1: the shared Crash runtime over its facts, plus its block pool and BIOS pad word.
class Runtime final : public crash::Runtime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  bool interpolates() const override {
    return true;
  }
};

} // namespace crash1
