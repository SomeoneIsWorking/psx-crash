#pragma once

#include "crash1/frame/draw_replay.h"
#include "crash1/render/block_pool.h"
#include "runtime/session.h"

namespace crash1 {

// The shared Crash session plus Crash 1's block pool and draw replay.
class Session final : public crash::Session {
public:
  using crash::Session::Session;

  BlockPool blockPool;
  DrawReplay drawReplay;
};

// The Crash 1 session of the Game `core` belongs to; aborts on another title's session.
Session &session(Core &core);

} // namespace crash1
