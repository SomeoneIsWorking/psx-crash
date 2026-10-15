#pragma once

#include <cstdint>

struct Core;

namespace crash1::bios_pad_input {

inline constexpr std::uint16_t kDisconnectedPort = 0xFFFFu;

// Publish the active-low PSX mask through Crash 1's BIOS PadRead word; port 1 stays disconnected.
std::uint32_t wordAddress();
void publishPrimary(Core &core, std::uint16_t activeLowButtons);

} // namespace crash1::bios_pad_input
