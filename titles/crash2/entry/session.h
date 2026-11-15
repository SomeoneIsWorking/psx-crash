#pragma once

#include "crash2/frame/pose_blend.h"
#include "crash2/render/morph_reader.h"
#include "runtime/session.h"

namespace crash2 {

// The shared Crash session plus Crash 2's poses at the object queue and the morph readers' weights.
class Session final : public crash::Session {
public:
  using crash::Session::Session;

  [[nodiscard]] crash::QueuePoses *queuePoses() override {
    return &poses;
  }

  PoseBlend poses;
  MorphReader morphReader;
};

// The Crash 2 session of the Game `core` belongs to; aborts on another title's session.
Session &session(Core &core);

} // namespace crash2
