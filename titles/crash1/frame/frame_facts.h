// frame_facts.h — Crash 1's frame loop and draw inputs. Names from external/crash-bandicoot-decomp
// (configs/USA/sym.main.txt, src/main/core/main.c CORE_Loop, src/main/render/frame_update.c Gpu_Update,
// src/main/gool/entries_AAE0.c GOOL_InitAllocTable, src/main/gool/physics.c GoolObj); code sites from the retail
// executable's disassembly.
#pragma once

#include <cstdint>

namespace crash1::frame_facts {

inline constexpr std::uint32_t kCoreLoop = 0x80011FC4u;
// CORE_Loop's jal Gfx_UpdateMatrices after Cam_Update: the picture half's first block.
inline constexpr std::uint32_t kMatricesCall = 0x800123BCu;
// CORE_Loop's jal Gool_UpdateObjects; the call returns to the Title_Update test.
inline constexpr std::uint32_t kObjectsReturn = 0x800124DCu;
// CORE_Loop's jal Gpu_Update, the iteration's last call.
inline constexpr std::uint32_t kGpuUpdateCall = 0x80012508u;
inline constexpr std::uint32_t kGoolUpdateObjects = 0x8001D5ECu;
inline constexpr std::uint32_t kGpuUpdate = 0x80016E5Cu;
// Gpu_Update's jal Audio_Update, after the fade overlay and the backdrop rectangles; DrawSync, the swap, the waits and
// DrawOTag follow.
inline constexpr std::uint32_t kAudioUpdateCall = 0x8001704Cu;
inline constexpr std::uint32_t kGpuRResetOt = 0x800342D8u;
// Pad_Update: the BIOS pad read, which GOOL_UpdateObject calls for the player before anything else; nothing reads its
// result back.
inline constexpr std::uint32_t kPadUpdate = 0x800167A4u;

// g_ot_cur: the render buffer being built. Gpu_Update ends each frame by setting its primitive cursor (+0x78) to its
// base (+0x74) and clearing its 0x800-entry ordering table (+0x88), which it hands DrawOTag a frame later.
inline constexpr std::uint32_t kOtCur = 0x8005840Cu;
inline constexpr std::uint32_t kPrimBase = 0x74u;
inline constexpr std::uint32_t kPrimCursor = 0x78u;
inline constexpr std::uint32_t kOrderingTable = 0x88u;
inline constexpr std::uint32_t kOrderingTableEntries = 0x800u;

// g_cur_lid_ro, the frame flags: GOOL_UpdateObject runs an object's code and physics only while bit 3 is set.
inline constexpr std::uint32_t kFrameFlags = 0x800618B0u;
inline constexpr std::uint32_t kObjectLogic = 0x8u;

// g_cam_trans_x/y/z (24.8), then g_cam_rot_x/y/z (4096 a turn): what Gfx_UpdateMatrices builds the view from.
inline constexpr std::uint32_t kCamera = 0x80057864u;

// gool_alloc.db0, the player's record, and gool_alloc.db4, the pool of 0x60 objects of 0x25C bytes; a record's first
// word is 1 while it is live (GOOL_InitObject) and 0 once killed.
inline constexpr std::uint32_t kPlayer = 0x80060DB0u;
inline constexpr std::uint32_t kObjectPool = 0x80060DB4u;
inline constexpr std::uint32_t kObjectStride = 0x25Cu;
inline constexpr std::uint32_t kObjectCount = 0x60u;

// GoolObj: exec, then vectors.trans (+0x80), rot (+0x8C, 4096 a turn) and scale (+0x98), anim_seq and anim_frame.
namespace object {
inline constexpr std::uint32_t kExec = 0x20u;
inline constexpr std::uint32_t kTrans = 0x80u;
inline constexpr std::uint32_t kRot = 0x8Cu;
inline constexpr std::uint32_t kScale = 0x98u;
inline constexpr std::uint32_t kAnimSeq = 0x108u;
inline constexpr std::uint32_t kAnimFrame = 0x10Cu;
} // namespace object

} // namespace crash1::frame_facts
