#pragma once

#include "frame/camera_cuts.h"
#include "frame/frame_cut.h"
#include "game_runtime.h"
#include "replay/draw_replay.h"
#include "replay/object_walk.h"
#include "replay/pose_blend.h"

#include <memory>
#include <optional>

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
  // The poses at the object queue; none draws each object's queued pose.
  [[nodiscard]] QueuePoses *queuePoses() {
    return poses_.get();
  }
  // The morph reader the poses weigh; aborts for a title without pose facts.
  [[nodiscard]] MorphReader &morphReader();
  // The camera cut declarations; aborts for a title without their facts.
  [[nodiscard]] CameraCuts &cameraCuts();

private:
  std::unique_ptr<DrawReplay> drawReplay_;
  std::unique_ptr<PoseBlend> poses_;
  std::optional<CameraCuts> cameraCuts_;
};

// The session of the Game `core` belongs to; aborts when it has none.
Session &session(Core &core);

} // namespace crash
