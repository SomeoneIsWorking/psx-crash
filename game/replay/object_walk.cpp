#include "replay/object_walk.h"

#include "core.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "runtime/session.h"

namespace crash {

void ObjectWalk::forEachList(Core &core, QueuePoses *poses) {
  // Obj_UpdateAll is the one caller that passes Obj_Update; a replay's runs inside a GuestRedraw.
  const bool update = core.r[psx::cpu::kA1] == facts_.objUpdate;
  if (update && core.writeRollback.armed()) {
    const std::uint32_t argument = core.r[psx::cpu::kA2];
    for (const std::uint32_t object : queued_) {
      // The real walk's later objects can page out this one's model entry, which the logic-off placement reads.
      if (poses != nullptr) {
        poses->fetchPose(core, object);
      }
      psx::cpu::callGuestNow(core, "Crash object walk", facts_.objUpdate, object, argument);
    }
    return;
  }
  if (update) {
    queued_.clear();
  }
  // A level load's walk can outrun the turn; its budget exit is the caller's, resumed at the next turn.
  (void)psx::cpu::callOriginalOrPropagate(core, facts_.objForEachList);
}

void ObjectWalk::queue(Core &core, QueuePoses *poses) {
  const std::uint32_t object = core.r[4];
  const std::uint32_t place = core.r[5];
  if (!core.writeRollback.armed()) {
    queued_.push_back(object);
  }
  if (poses != nullptr) {
    poses->beforeQueue(core, object);
  }
  const std::uint32_t frame = core.mem_r32(facts_.buildBuffer);
  const std::uint32_t head = core.mem_r32(frame + facts_.packetHead);
  core.r[4] = object;
  core.r[5] = place;
  psx::cpu::callOriginalToReturn(
      core, facts_.queueObject, psx::cpu::ExecutionBudget::currentTurn(core), "Crash queue object");
  if (const std::uint32_t packet = core.mem_r32(frame + facts_.packetHead); packet != head && poses != nullptr) {
    poses->queued(core, packet);
  }
}

void ObjectWalk::forEachListOverride(Core *core) {
  Session &owner = session(*core);
  owner.drawReplay().walk().forEachList(*core, owner.queuePoses());
}

void ObjectWalk::queueOverride(Core *core) {
  Session &owner = session(*core);
  owner.drawReplay().walk().queue(*core, owner.queuePoses());
}

void ObjectWalk::registerOverrides(Core &core, const ReplayFacts &facts) {
  psx::cpu::installNativeOverride(core, facts.objForEachList, "Crash object walk", forEachListOverride);
  psx::cpu::installNativeOverride(core, facts.queueObject, "Crash object queue", queueOverride);
}

} // namespace crash
