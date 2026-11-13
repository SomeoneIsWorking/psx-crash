#include "debug/debug_menu.h"

#include "core.h"
#include "frame/frame_driver.h"
#include "frame/scene_warp.h"
#include "game.h"

#include <lucent/log.h>

#include <vector>

namespace crash {
namespace {

std::string loopNotRunning(Core &core) {
  const auto *driver = core.game->frameDriverIf<FrameDriver>();
  return driver != nullptr && driver->warpAllowed() ? std::string() : "a warp is legal only once the game loop is running";
}

} // namespace

DebugMenu::DebugMenu() {
  warp({
      .levels = {"scene"},
      .places =
          [](Core &core) {
            std::vector<psx::game::WarpPlace> scenes;
            const auto *driver = core.game->frameDriverIf<FrameDriver>();
            if (driver == nullptr || !driver->warpAllowed()) {
              return scenes;
            }
            const SceneTable &table = driver->program().sceneTable;
            for (std::uint32_t scene = 0; scene < table.rows; ++scene) {
              if (!SceneWarp::hasScene(core, table, scene)) {
                continue;
              }
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
      .refusal = loopNotRunning,
  });
  verb({
      .name = "spawn",
      .summary = "re-enter the current level at one of its spawn points",
      .params = {psx::game::DebugParam::integer("point", 0, 0xFF)},
      .run =
          [](const psx::game::DebugCall &call) {
            call.reply = call.core.game->frameDriverAs<FrameDriver>().armSpawn(call.core, call.values.u32(0));
          },
      .refusal = loopNotRunning,
  });
}

} // namespace crash
