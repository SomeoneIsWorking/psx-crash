// pose_blend.h — an object's pose in a replay drawn halfway between its two ticks' poses by the morph drawer.
//
// func_8001A4A4 fetches an object's two vertex frames, but only its logic calls it; Gfx_QueueObject sends a pair that
// differs to the morph drawer (crash2-decomp docs/notes/frame-loop.md, Animation). At the queue, a t = 1/2 replay
// refetches both ticks' pairs; when those four frames are at most two frames, that pair is drawn with k quarters on
// the second (crash2::MorphReader, keyed by the queued packet). Any other pose, t or frame of another model is drawn as
// the later tick's.
#pragma once

class Core;

namespace crash2::pose_blend {

void registerOverride(Core &core);

} // namespace crash2::pose_blend
