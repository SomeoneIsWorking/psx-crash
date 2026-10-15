#include "crash1/frame/frame_driver.h"

#include "core.h"
#include "crash1/input/bios_pad_input.h"
#include "game.h"

namespace crash1 {

void FrameDriver::publishInput(Core &core) {
  bios_pad_input::publishPrimary(core, game().pad.buttons);
}

} // namespace crash1
