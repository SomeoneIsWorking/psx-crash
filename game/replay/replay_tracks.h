// replay_tracks.h — what every Crash draw replay blends by address: the camera's position and angles, and each live
// GOOL object's position, angles, scale and animation, paired by the object's address and executable.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace crash {

// A GOOL object's fields (Crash 1 GoolObj, Crash 2 and 3 GoolObject): its executable, position (three words), angles
// (three, 4096 a turn), scale (three, x negated to flip), animation and animation frame.
struct ObjectLayout {
  std::uint32_t exec;
  std::uint32_t trans;
  std::uint32_t rot;
  std::uint32_t scale;
  std::uint32_t anim;
  std::uint32_t animFrame;
};

// The camera's position (three words, 24.8) followed by its angles (three, 4096 a turn); the word holding the object
// pool's address, its records' stride and count; the word holding the player's record, kept apart from the pool. A
// record's first word is 1 while it holds an object.
struct TrackFacts {
  std::uint32_t camera;
  std::uint32_t objectPool;
  std::uint32_t objectStride;
  std::uint32_t objectCount;
  std::uint32_t player;
  ObjectLayout object;
};

class ReplayTracks {
public:
  explicit ReplayTracks(const TrackFacts &facts);
  ReplayTracks(const ReplayTracks &) = delete;
  ReplayTracks &operator=(const ReplayTracks &) = delete;
  ReplayTracks(ReplayTracks &&) = delete;
  ReplayTracks &operator=(ReplayTracks &&) = delete;
  ~ReplayTracks() = default;

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }
  // The pool's track and the player's.
  [[nodiscard]] std::span<const psx::present::MotionTrack> objectTracks() const {
    return std::span(tracks_).subspan(1);
  }
  // The animation fields' places in an object's samples.
  static constexpr std::size_t kAnimField = 9;
  static constexpr std::size_t kAnimFrameField = 10;

private:
  // Position and angles by their fields' motion; a scale whose sign changes (a flip) steps.
  class Placement final : public psx::present::MotionRule {
  public:
    explicit Placement(std::span<const psx::present::MotionField> fields) : fields_(fields) {}
    void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;

  private:
    std::span<const psx::present::MotionField> fields_;
  };

  std::array<psx::present::MotionField, 6> camera_;
  std::array<psx::present::MotionField, 11> object_;
  // The executable: a slot freed and reused by another kind does not pair.
  std::array<psx::present::MotionKey, 1> keys_;
  Placement placement_;
  std::array<psx::present::MotionTrack, 3> tracks_;
};

} // namespace crash
