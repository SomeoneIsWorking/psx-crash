#include "crash2/frame/object_walk.h"

#include "core.h"
#include "crash2/entry/session.h"
#include "crash2/frame/pose_blend.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "r3000.h"

namespace crash2 {
namespace {

// Names from external/crash2-decomp (config/symbol_addrs.txt, docs/notes/frame-loop.md).
constexpr std::uint32_t kObjForEachList = 0x80018DD0u;
constexpr std::uint32_t kObjUpdate = 0x8001C718u;

} // namespace

void ObjectWalk::queued(const Core &core, std::uint32_t object) {
  if (!core.writeRollback.armed()) {
    queued_.push_back(object);
  }
}

void ObjectWalk::forEachList(Core *core) {
  ObjectWalk &walk = session(*core).drawReplay.walk();
  // Obj_UpdateAll is the one caller that passes Obj_Update; a replay's runs inside a GuestRedraw.
  const bool update = core->r[psx::cpu::kA1] == kObjUpdate;
  if (update && core->writeRollback.armed()) {
    const std::uint32_t argument = core->r[psx::cpu::kA2];
    for (const std::uint32_t object : walk.queued_) {
      // The real walk's later objects can page out this one's model entry, which the logic-off placement reads.
      pose_blend::fetchPose(*core, object);
      psx::cpu::callGuestNow(*core, "Crash 2 object walk", kObjUpdate, object, argument);
    }
    return;
  }
  if (update) {
    walk.queued_.clear();
  }
  // A level load's walk can outrun the turn; its budget exit is the caller's, resumed at the next turn.
  (void)psx::cpu::callOriginalOrPropagate(*core, kObjForEachList);
}

void ObjectWalk::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kObjForEachList, "Crash 2 object walk", forEachList);
}

} // namespace crash2
