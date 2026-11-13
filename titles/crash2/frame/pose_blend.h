// pose_blend.h — an object's pose in a replay drawn halfway between its two ticks' poses by the retail morph drawer.
//
// func_8001A90C fetches an object's two vertex frames; Gfx_QueueObject sends a pair that differs to the morph drawer,
// which adds them at equal weight (crash2-decomp docs/notes/frame-loop.md, Animation). At t = 1/2 the halfway pose
// is a quarter each of the two ticks' pairs; when those four frames are two frames twice (or one), the replay's fetch
// returns that pair. Any other pose, t or frame of another model is drawn as the later tick's.
#pragma once

class Core;

namespace crash2::pose_blend {

void registerOverride(Core &core);

} // namespace crash2::pose_blend
