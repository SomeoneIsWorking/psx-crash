#pragma once

#include "frame/frame_cut.h"
#include "game_runtime.h"
#include "replay/draw_replay.h"
#include "replay/object_walk.h"

#include <memory>

class Core;

namespace crash {

struct TitleFacts;

// What one Core of a Crash title owns outside guest RAM, created with its Game.
class Session : public psx::game::TitleSessionState {
public:
  explicit Session(const TitleFacts &facts);

  FrameCut frameCut;

  // The shared draw replay; aborts for a title without its facts.
  [[nodiscard]] DrawReplay &drawReplay();
  [[nodiscard]] bool hasDrawReplay() const {
    return drawReplay_ != nullptr;
  }
  // The title's poses at the object queue; none draws each object's queued pose.
  [[nodiscard]] virtual QueuePoses *queuePoses() {
    return nullptr;
  }

private:
  std::unique_ptr<DrawReplay> drawReplay_;
};

// The session of the Game `core` belongs to; aborts when it has none.
Session &session(Core &core);

} // namespace crash
