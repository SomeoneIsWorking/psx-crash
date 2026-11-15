#include "runtime/session.h"

#include "core.h"
#include "game.h"
#include "runtime/title_facts.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash {

Session::Session(const TitleFacts &facts)
    : frameCut(facts.frame), drawReplay_(facts.replay ? std::make_unique<DrawReplay>(*facts.replay) : nullptr),
      poses_(facts.poses ? std::make_unique<PoseBlend>(*facts.poses) : nullptr) {
  if (facts.cameraCuts) {
    cameraCuts_.emplace(*facts.cameraCuts);
  }
}

CameraCuts &Session::cameraCuts() {
  if (!cameraCuts_) {
    lucent::error("crash-runtime", "a camera cut override ran for a title without camera cut facts");
    std::abort();
  }
  return *cameraCuts_;
}

MorphReader &Session::morphReader() {
  if (!poses_) {
    lucent::error("crash-runtime", "the morph reader ran for a title without pose facts");
    std::abort();
  }
  return poses_->reader();
}

DrawReplay &Session::drawReplay() {
  if (!drawReplay_) {
    lucent::error("crash-runtime", "the shared draw replay ran for a title without its facts");
    std::abort();
  }
  return *drawReplay_;
}

Session &session(Core &core) {
  if (core.game == nullptr || !core.game->sessionState) {
    lucent::error("crash-runtime", "Session is absent from the Game");
    std::abort();
  }
  return static_cast<Session &>(*core.game->sessionState);
}

} // namespace crash
