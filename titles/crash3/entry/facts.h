#pragma once

#include "runtime/title_facts.h"
#include "title_catalog.h"

namespace crash3 {

// Crash 3 (SCUS-942.44): every measured address the shared native boot and frame owners consume.
const crash::TitleFacts &facts();

// The serial-identified executable, from executable.json.
const psx::host::TitleIdentity &identity();

} // namespace crash3
