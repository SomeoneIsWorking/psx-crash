#include "crash2/entry/session.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash2 {

Session &session(Core &core) {
  auto *const owned = dynamic_cast<Session *>(&crash::session(core));
  if (owned == nullptr) {
    lucent::error("crash2-runtime", "a Crash 2 owner ran on another title's session");
    std::abort();
  }
  return *owned;
}

} // namespace crash2
