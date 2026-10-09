#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace crash {

// Whether a sealed frame starts a new scene, from the guest's own scene words.
class FrameCut {
public:
  FrameCut(std::uint32_t sceneIdAddress, std::uint32_t sceneRequestAddress);

  // Called once per logic frame, after its iteration and before the record is sealed.
  void observe(Core &core);

  // A new session's first sealed frame is a cut.
  void reset();

  bool isCut() const {
    return cut_;
  }

private:
  std::uint32_t sceneIdAddress_;
  std::uint32_t sceneRequestAddress_;
  std::optional<std::uint32_t> scene_;
  bool requestPending_ = false;
  bool cut_ = true;
};

} // namespace crash
