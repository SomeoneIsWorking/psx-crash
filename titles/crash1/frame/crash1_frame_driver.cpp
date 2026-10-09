#include "crash1_frame_driver.h"

#include "core.h"
#include "crash1_bios_pad_input.h"
#include "game.h"

namespace crash1 {

void Crash1FrameDriver::publishInput(Core &core) {
  bios_pad_input::publishPrimary(core, game().pad.buttons);
}

} // namespace crash1
