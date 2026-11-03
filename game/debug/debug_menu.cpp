#include "debug/debug_menu.h"

#include "core.h"
#include "frame/frame_driver.h"
#include "frame/scene_warp.h"
#include "game.h"

#include <lucent/log.h>

#include <vector>

namespace crash {

DebugMenu::DebugMenu() {
  warp({
      .levels = {"scene"},
      .places =
          [](Core &) {
            std::vector<psx::game::WarpPlace> scenes;
            for (std::uint32_t scene = 0; scene <= SceneWarp::kLastScene; ++scene) {
              scenes.push_back({
                  .name = lucent::format("scene 0x{:02X}", scene),
                  .arrive =
                      [scene](Core &core) {
                        return core.game->frameDriverAs<FrameDriver>().armWarp(core, scene);
                      },
              });
            }
            return scenes;
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
