#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace crash1 {

// Whether a sealed frame starts a new scene, from the guest's own scene words.
class Crash1FrameCut {
public:
  // Called once per logic frame, after its iteration and before the record is sealed.
  void observe(Core &core);

  bool isCut() const {
    return cut_;
  }

private:
  std::optional<std::uint32_t> scene_;
  bool requestPending_ = false;
  bool cut_ = true;
};

} // namespace crash1
