#pragma once

#include "game_runtime.h"

#include <string_view>

namespace crash {

// Framework integration for titles whose execution frontier is not yet a native boot.
class BoundaryRuntime : public GameRuntime {
public:
  RenderCapabilities renderCapabilities() const final;
  void *createContext(Core &core) final;
  void destroyContext(void *context) final;
  void registerOverrides(Game &game) final;
  void bootInit(Core &core) final;

protected:
  BoundaryRuntime(std::string_view logDomain, std::string_view blockedReason);

private:
  std::string_view logDomain_;
  std::string_view blockedReason_;
};

} // namespace crash
