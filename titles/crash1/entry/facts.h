#pragma once

#include "runtime/title_facts.h"
#include "title_catalog.h"

namespace crash1 {

// Crash 1 (SCUS-949.00): every measured address the shared native boot and frame owners consume.
const crash::TitleFacts &facts();

// The serial-identified executable, from executable.json.
const psx::host::TitleIdentity &identity();

} // namespace crash1
