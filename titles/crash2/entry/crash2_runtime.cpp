#include "crash2_runtime.h"

#include "crash2_facts.h"

namespace crash2 {

Crash2Runtime::Crash2Runtime() : CrashRuntime(crash2::facts(), widescreen_, "crash2") {}

void Crash2Runtime::registerTitleOverrides(Game &) {}

} // namespace crash2
