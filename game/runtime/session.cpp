#include "runtime/session.h"

#include "core.h"
#include "game.h"
#include "runtime/title_facts.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash {

Session::Session(const TitleFacts &facts) : frameCut(facts.frame) {}

Session &session(Core &core) {
  if (core.game == nullptr || !core.game->sessionState) {
    lucent::error("crash-runtime", "Session is absent from the Game");
    std::abort();
  }
  return static_cast<Session &>(*core.game->sessionState);
}

} // namespace crash
