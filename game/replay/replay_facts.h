// replay_facts.h — where a Crash 2 or Crash 3 executable keeps what its draw replay runs and blends: the main loop's
// picture half and the end of frame's draws, the view, fade, object walk and queue entries, and the tracked records.
#pragma once

#include "replay/replay_tracks.h"

#include <cstdint>
#include <optional>

namespace crash {

struct ReplayFacts {
  // Sys_MainLoop, and in it: the jal Gfx_UpdateView after Cam_Update (the picture half's first block), the
  // Obj_UpdateAll argument load, the first block past the Sys_BeforeDrawObjects call, and the jal of the end of frame.
  std::uint32_t mainLoop;
  std::uint32_t viewCall;
  std::uint32_t objectsCall;
  std::uint32_t drawObjects;
  std::uint32_t endFrameCall;
  // Sys_BeforeDrawObjects, the empty function called between the object walk and Gfx_DrawObjects: the sample point.
  std::uint32_t sampleStub;
  std::uint32_t objUpdateAll;
  // Gfx_ResetFrame(frame): the primitive cursor to its base, the object queue emptied.
  std::uint32_t resetFrame;
  // The end of frame (Crash 2 Gfx_EndFrame, Crash 3 GpuUpdate), its argument when it takes one, and the block where
  // its sound update and device tail begin.
  std::uint32_t endFrame;
  std::optional<std::uint32_t> endFrameArgument;
  std::uint32_t endFrameDeviceTail;
  // g_BuildBuffer, and its GfxFrame's ordering table and object packet list head.
  std::uint32_t buildBuffer;
  std::uint32_t orderingTable;
  std::uint32_t packetHead;
  std::uint32_t paused;
  // g_FrameFlags: Obj_Update runs an object's logic only while `objectLogic` is set; `additiveFade` picks the fade's
  // blend; `objectsFirst`, when nonzero, is the bit that moves Obj_UpdateAll before the world and skips the view.
  std::uint32_t frameFlags;
  std::uint32_t objectLogic;
  std::uint32_t additiveFade;
  std::uint32_t objectsFirst;
  // Gfx_UpdateView and what it steps while unpaused: the screen shake (24.8, its sign alternating as it decays) and,
  // when the title scrolls streaks there, their last and current scroll (two u16, a word, then the same three).
  std::uint32_t updateView;
  std::uint32_t screenShake;
  std::optional<std::uint32_t> streakScroll;
  // Gfx_DrawFade(level), which takes its draw mode and flat quad at the build buffer's primitive cursor.
  std::uint32_t drawFade;
  // Obj_ForEachList(fn, ...), Obj_Update (the function Obj_UpdateAll hands it) and Gfx_QueueObject(object, place).
  std::uint32_t objForEachList;
  std::uint32_t objUpdate;
  std::uint32_t queueObject;
  TrackFacts tracks;
};

} // namespace crash
