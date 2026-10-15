#include "crash3/entry/runtime.h"

#include "crash3/entry/facts.h"

namespace crash3 {

Runtime::Runtime() : crash::Runtime(crash3::facts()) {}

} // namespace crash3
