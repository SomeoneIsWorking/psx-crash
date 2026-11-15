// pose_blend.h — an object's pose in a replay drawn halfway between its two ticks' poses by the morph drawer.
//
// The pose fetch (Crash 2 func_8001A4A4, Crash 3 0x8001B0BC) puts an object's two vertex frames in its block, but only
// its logic calls it; Gfx_QueueObject sends a pair that differs to the morph drawer (crash2-decomp and crash3-decomp
// docs/notes/frame-loop.md, Animation). At the queue, a t = 1/2 replay
// refetches both ticks' pairs; when those four frames are at most two frames, that pair is drawn with k quarters on
// the second, and three or four frames are drawn as one pair with the other decoded beside it, a quarter on each frame
// (crash::MorphReader, keyed by the queued packet). Any other t, or a frame of another model, is drawn as the later
// tick's.
#pragma once

#include "render/morph_reader.h"
#include "replay/object_walk.h"

#include <cstdint>

class Core;

namespace crash {

// Where one title keeps the pose fetch and the object fields it reads and writes.
struct PoseFacts {
  // fetch(object, object + scale).
  std::uint32_t fetchPose;
  std::uint32_t block;
  std::uint32_t scale;
  std::uint32_t anim;
  std::uint32_t animFrame;
  // The block's two vertex frames, which Gfx_QueueObject's vertex kind draws.
  std::uint32_t firstFrame;
  std::uint32_t secondFrame;
  std::uint8_t vertexKind;
  // A vertex frame's model entry id, which both frames of a morph must share.
  std::uint32_t frameModel;
  MorphFacts reader;
};

class PoseBlend final : public QueuePoses {
public:
  explicit PoseBlend(const PoseFacts &facts) : facts_(facts), reader_(facts.reader) {}

  [[nodiscard]] MorphReader &reader() {
    return reader_;
  }

  void beforeQueue(Core &core, std::uint32_t object) override;
  void queued(Core &core, std::uint32_t packet) override;
  // The pose fetch for a vertex object's current pose, which the logic paths run before the object places itself.
  void fetchPose(Core &core, std::uint32_t object) override;

private:
  MorphReader::Mix blendPose(Core &core, std::uint32_t object) const;

  const PoseFacts &facts_;
  MorphReader reader_;
  // The mix the object being queued takes.
  MorphReader::Mix mix_;
};

} // namespace crash
