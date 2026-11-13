// object_walk.h — the objects the real Obj_UpdateAll queued, in its order: an object spawned during the walk where the
// walk reached it, one moved to another list once each time the walk met it. A replay's walk runs Obj_Update on the
// same objects in the same order, so it queues what the real frame queued.
#pragma once

#include <cstdint>
#include <vector>

class Core;

namespace crash2 {

class ObjectWalk {
public:
  // Obj_ForEachList, when Obj_UpdateAll hands it Obj_Update.
  static void registerOverrides(Core &core);

  // Gfx_QueueObject's one caller is Obj_Update's tail: `object` was queued.
  void queued(const Core &core, std::uint32_t object);

private:
  static void forEachList(Core *core);

  std::vector<std::uint32_t> queued_;
};

} // namespace crash2
