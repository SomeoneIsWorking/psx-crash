#include "crash2/entry/runtime.h"

#include "crash2/entry/facts.h"

namespace crash2 {

Runtime::Runtime() : crash::Runtime(crash2::facts()) {}

} // namespace crash2
