// pose_blend.h — an object's vertex-animated pose in a Crash 1 in-between. Gool_ObjectTransform hands
// Gfx_TransformSvtx one SVTX frame (Gfx_TransformCvtx one CVTX frame), the object's animation frame as the tick left
// it, so a replay would draw the later tick's pose. Each real object pass records the frame every object drew; a
// replay's call draws a frame t of the way from the object's last real frame to its own, written over the frame for
// that call only.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

class Core;

namespace crash1 {

class PoseBlend {
public:
  // The real object pass starts or ends; the in-betweens replayed after it blend from the pass before it.
  void openPass();
  void closePass();
  // Gfx_TransformSvtx and Gfx_TransformCvtx(frame, ot, object), from DrawReplay.
  static void registerOverrides(Core &core);
  // Run end: frames drawn blended, and drawn as the later tick because the object drew no matching frame last tick.
  void report() const;

private:
  // The tail bytes after a vertex's position: SVTX's signed normals or CVTX's unsigned colours.
  enum class Kind : std::uint8_t { Svtx, Cvtx };
  struct Frame {
    std::uint32_t object = 0;
    std::uint32_t exec = 0;
    Kind kind = Kind::Svtx;
    std::vector<std::uint8_t> bytes;
  };
  static void svtxOverride(Core *core);
  static void cvtxOverride(Core *core);
  void transform(Core &core, Kind kind, std::uint32_t frame, std::uint32_t object, std::optional<float> t);
  [[nodiscard]] const Frame *drawnFrame(std::uint32_t object) const;

  bool recording_ = false;
  // The frames the previous real pass drew, and the running or last one's.
  std::vector<Frame> drawn_;
  std::vector<Frame> drawing_;
  // By Kind.
  std::array<std::uint64_t, 2> blended_{};
  std::array<std::uint64_t, 2> unpaired_{};
};

} // namespace crash1
