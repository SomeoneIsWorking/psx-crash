#pragma once

#include "frame/frame_cut.h"
#include "game_runtime.h"

class Core;

namespace crash {

struct TitleFacts;

// What one Core of a Crash title owns outside guest RAM, created with its Game.
class Session : public psx::game::TitleSessionState {
public:
  explicit Session(const TitleFacts &facts);

  FrameCut frameCut;
};

// The session of the Game `core` belongs to; aborts when it has none.
Session &session(Core &core);

} // namespace crash
