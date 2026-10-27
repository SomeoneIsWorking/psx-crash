#include "debug/debug_menu.h"

#include "core.h"
#include "frame/frame_driver.h"
#include "frame/scene_warp.h"
#include "game.h"

namespace crash {

DebugMenu::DebugMenu() {
  using psx::game::DebugCall;
  using psx::game::DebugParam;
  section("Scene Warp");
  verb({
      .name = "warp",
      .summary = "Load that scene",
      .params = {DebugParam::hex("scene", 0, SceneWarp::kLastScene)},
      .run =
          [](const DebugCall &call) {
            call.reply = call.core.game->frameDriverAs<FrameDriver>().armWarp(call.core, call.values.u32(0));
          },
      .refusal =
          [](Core &core) {
            const auto *driver = core.game->frameDriverIf<FrameDriver>();
            return driver != nullptr && driver->warpAllowed() ? std::string()
                                                              : "a warp is legal only once the game loop is running";
          },
  });
}

} // namespace crash
