#pragma once

#include "core.h"

#include <cstdint>

namespace crash {

// Runs one guest leaf; injected so a boot owner's call sequence is testable without guest code.
using LeafDispatch = void (*)(Core *, std::uint32_t);

// One guest call a native boot owner makes for the retail body: the leaf and the return address inside that body.
struct LeafCall {
  std::uint32_t leaf;
  std::uint32_t returnAddress;
};

inline void callLeaf(Core &core, LeafDispatch dispatch, LeafCall call) {
  core.r[31] = call.returnAddress;
  dispatch(&core, call.leaf);
}

} // namespace crash
