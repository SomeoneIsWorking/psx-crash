#pragma once

#include "crash2/frame/draw_replay.h"
#include "runtime/session.h"

namespace crash2 {

// The shared Crash session plus Crash 2's draw replay.
class Session final : public crash::Session {
public:
  using crash::Session::Session;

  DrawReplay drawReplay;
};

// The Crash 2 session of the Game `core` belongs to; aborts on another title's session.
Session &session(Core &core);

} // namespace crash2
