#include "crash1/entry/session.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash1 {

Session &session(Core &core) {
  auto *const owned = dynamic_cast<Session *>(&crash::session(core));
  if (owned == nullptr) {
    lucent::error("crash1-runtime", "a Crash 1 owner ran on another title's session");
    std::abort();
  }
  return *owned;
}

} // namespace crash1
