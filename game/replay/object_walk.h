// object_walk.h — the objects the real Obj_UpdateAll queued, in its order: an object spawned during the walk where the
// walk reached it, one moved to another list once each time the walk met it. A replay's walk runs Obj_Update on the
// same objects in the same order, so it queues what the real frame queued. Gfx_QueueObject reports each queue, and a
// title's pose owner (QueuePoses) sees each queued object and the packet it got.
#pragma once

#include "replay/replay_facts.h"

#include <cstdint>
#include <vector>

class Core;

namespace crash {

// A title's object poses at the queue: what a replay writes before Gfx_QueueObject reads the object, and the packet it
// queued; and the pose fetch the logic paths run before an object places itself.
class QueuePoses {
public:
  virtual ~QueuePoses() = default;
  virtual void beforeQueue(Core &core, std::uint32_t object) = 0;
  virtual void queued(Core &core, std::uint32_t packet) = 0;
  virtual void fetchPose(Core &core, std::uint32_t object) = 0;
};

class ObjectWalk {
public:
  explicit ObjectWalk(const ReplayFacts &facts) : facts_(facts) {}

  // Obj_ForEachList and Gfx_QueueObject.
  static void registerOverrides(Core &core, const ReplayFacts &facts);

private:
  static void forEachListOverride(Core *core);
  static void queueOverride(Core *core);
  void forEachList(Core &core, QueuePoses *poses);
  void queue(Core &core, QueuePoses *poses);

  const ReplayFacts &facts_;
  std::vector<std::uint32_t> queued_;
};

} // namespace crash
