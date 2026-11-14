#include "crash1/render/object_cull.h"

#include "core.h"
#include "execution_control.h"
#include "execution_services.h"
#include "game.h"
#include "guest_call.h"
#include "guest_redraw.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "render/world_screen.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>
#include <optional>
#include <string_view>

namespace crash1 {
namespace {

using namespace psx::cpu;

inline constexpr std::string_view kOwner = "Crash 1 object cull";

// Gfx_CalcObjectMatrices; the original body resumes where the test keeps the object or the level skips it.
inline constexpr std::uint32_t kEntry = 0x800180CCu;
inline constexpr std::uint32_t kKept = 0x8001838Cu;
inline constexpr std::uint32_t kUntested = 0x800188ACu;
inline constexpr GuestCallSite kSetRotMatrix = GuestCallSite::fromJal(0x80018124u, 0x80042E9Cu);
inline constexpr GuestCallSite kSetTransMatrix = GuestCallSite::fromJal(0x80018134u, 0x80042F2Cu);
inline constexpr GuestCallSite kRotTrans = GuestCallSite::fromJal(0x8001818Cu, 0x80042FECu);
inline constexpr GuestCallSite kProjectBound = GuestCallSite::fromJal(0x80018260u, 0x800351C8u);

// Globals: the world view rotation and translation, the camera eye, the projection distance H, the level's flags
// and the zone the camera is in.
inline constexpr std::uint32_t kViewRotation = 0x80057844u;
inline constexpr std::uint32_t kViewTranslation = 0x8005726Cu;
inline constexpr std::uint32_t kEye = 0x80057888u;
inline constexpr std::uint32_t kProjection = 0x800578D0u;
inline constexpr std::uint32_t kLevelFlags = 0x800618B0u;
inline constexpr std::uint32_t kUntestedLevel = 0x10000u;
inline constexpr std::uint32_t kCameraZone = 0x80057914u;
inline constexpr std::uint32_t kZoneHeader = 0x10u;
inline constexpr std::uint32_t kFarClip = 0x2E8u;
inline constexpr std::int32_t kDefaultFar = 0x2EE0;

// The object: its bound box, position and status word.
inline constexpr std::uint32_t kObjectBound = 0x08u;
inline constexpr std::uint32_t kObjectPosition = 0x80u;
inline constexpr std::uint32_t kObjectStatus = 0xCCu;
inline constexpr std::uint32_t kAlwaysVisible = 0x40000u;

// The frame: saved registers, spilled arguments and the locals the body after the test reads.
inline constexpr std::uint32_t kFrameSize = 0xC8u;
inline constexpr std::uint32_t kFifthArgument = kFrameSize + 0x10u;
inline constexpr std::uint32_t kViewVector = 0x10u;
inline constexpr std::uint32_t kViewPosition = 0x2Cu;
inline constexpr std::uint32_t kFar = 0x88u;
inline constexpr std::uint32_t kSavedA1 = 0x90u;
inline constexpr std::uint32_t kSavedA3 = 0x98u;
struct Saved {
  int reg;
  std::uint32_t offset;
};
inline constexpr std::array<Saved, 10> kSaved{{{kS0, 0xA0u},
                                               {kS1, 0xA4u},
                                               {kS2, 0xA8u},
                                               {kS3, 0xACu},
                                               {kS4, 0xB0u},
                                               {kS5, 0xB4u},
                                               {kS6, 0xB8u},
                                               {kS7, 0xBCu},
                                               {kFp, 0xC0u},
                                               {kRa, 0xC4u}}};

// The box test reads the projected bound's extents Gte_RProjectBound leaves in the scratchpad (centred columns).
inline constexpr std::uint32_t kExtents = 0x1F800048u;
inline constexpr std::int32_t kRetailHalfWidth = crash::world::kScreenColumns / 2;
inline constexpr std::int32_t kRowsAbove = -0x6C;
inline constexpr std::int32_t kRowsBelow = 0x6C;
// The centre test: |sx| below 0x13D, sy + 0x8A below 0x147.
inline constexpr std::int32_t kCentreSlack = 0x13D - kRetailHalfWidth;
inline constexpr std::uint32_t kCentreRowsBias = 0x8Au;
inline constexpr std::uint32_t kCentreRows = 0x147u;

// Instructions the retail head executes on each stretch, between its calls.
struct Costs {
  std::uint32_t entry = 24u;
  std::uint32_t entryNoOut = 23u;
  std::uint32_t afterRotation = 4u;
  std::uint32_t afterTranslation = 22u;
  std::uint32_t levelFlag = 6u;
  std::uint32_t farLoad = 10u;
  std::uint32_t farTest = 5u;
  std::uint32_t farKept = 2u;
  std::uint32_t defaultFarTest = 5u;
  std::uint32_t alwaysVisible = 5u;
  std::uint32_t nearTest = 8u;
  std::uint32_t kind = 2u;
  std::uint32_t boxCall = 9u;
  std::uint32_t boxCallNoA3 = 1u;
  std::uint32_t boxLoad = 11u;
  std::uint32_t pairTest = 2u;
  std::uint32_t rightTest = 3u;
  std::uint32_t rowsBelowFirst = 3u;
  std::uint32_t rowsBelowSecond = 2u;
  std::uint32_t rowsRejected = 2u;
  std::uint32_t centreTest = 31u;
  std::uint32_t centreNegative = 1u;
  std::uint32_t centreRows = 3u;
  std::uint32_t rejectJump = 2u;
  std::uint32_t epilogue = 13u;
};
inline constexpr Costs kCosts{};

// The two callers, by the return site of their call: where each keeps its own return address.
struct Caller {
  std::uint32_t site = 0;
  std::uint32_t savedRa = 0;
};
inline constexpr std::array<Caller, 2> kCallers{{{0x800189B4u, 0x3Cu}, {0x80018B08u, 0x44u}}};

// The current draw context: its primitive buffer's head and tail and its 2048-slot ordering table.
inline constexpr std::uint32_t kContext = 0x8005840Cu;
inline constexpr std::uint32_t kPrimsHead = 0x74u;
inline constexpr std::uint32_t kPrimsTail = 0x78u;
inline constexpr std::uint32_t kOt = 0x88u;
inline constexpr std::uint32_t kOtBytes = 0x800u * 4u;
// Gpu_SetupPrims sizes the buffer by the level: 0x19 and 0x38 get more.
inline constexpr std::uint32_t kLevel = 0x80056710u;
inline constexpr std::uint32_t kPrimsBytes = 0x16000u;
inline constexpr std::uint32_t kPrimsBytes19 = 0x1E000u;
inline constexpr std::uint32_t kPrimsBytes38 = 0x1CC00u;

enum class Verdict : std::uint8_t {
  Kept,
  Untested,
  Rejected,
  // Rejected by the retail columns alone, kept by the canvas's.
  Margin,
};

// Columns in the test's centred units: an object is kept when it reaches [left, right].
struct Columns {
  std::int32_t left = 0;
  std::int32_t right = 0;
};

class Head {
public:
  explicit Head(Core &core) : c_(core) {
    const psx::present::CullWindow window =
        psx::present::recordCanvasColumns(core.game->guestDisplay.plan(), crash::world::kScreenColumns);
    canvas_ = {.left = window.left - kRetailHalfWidth, .right = window.right - kRetailHalfWidth};
  }

  void run();

private:
  std::uint32_t word(std::uint32_t address) const {
    return c_.mem_r32(address);
  }
  std::int32_t signedWord(std::uint32_t address) const {
    return static_cast<std::int32_t>(c_.mem_r32(address));
  }
  void charge(std::uint32_t instructions) {
    accountGuestInstructions(c_, instructions);
  }
  // The registers the retail head holds from its prologue on.
  void holdBodyRegisters();
  Verdict test();
  Verdict boxTest();
  Verdict centreTest();
  void reject(std::uint32_t cost);
  void resume(std::uint32_t pc);
  void drawMargin();

  Core &c_;
  Columns canvas_;
  std::array<std::uint32_t, 32> entry_{};
  std::uint32_t frame_ = 0;
  std::uint32_t object_ = 0;
  std::uint32_t out_ = 0;
  // The retail head's v1 when it returns: the last value it loaded there.
  std::uint32_t v1_ = 0;
};

void Head::holdBodyRegisters() {
  c_.r[kSp] = frame_;
  c_.r[kS0] = out_;
  c_.r[kS1] = entry_[kA0];
  c_.r[kS2] = entry_[kA0];
  c_.r[kS4] = object_;
  c_.r[kFp] = object_ + kObjectPosition;
}

void Head::run() {
  std::copy_n(c_.r, entry_.size(), entry_.begin());
  object_ = entry_[kA2];
  frame_ = entry_[kSp] - kFrameSize;
  out_ = word(entry_[kSp] + 0x10u);
  for (const Saved &saved : kSaved) {
    c_.mem_w32(frame_ + saved.offset, entry_[saved.reg]);
  }
  c_.mem_w32(frame_ + kSavedA1, entry_[kA1]);
  c_.mem_w32(frame_ + kSavedA3, entry_[kA3]);
  holdBodyRegisters();
  if (out_ != 0u) {
    c_.mem_w32(out_, 0u);
  }
  charge(out_ != 0u ? kCosts.entry : kCosts.entryNoOut);
  callGuestNow(c_, kOwner, kSetRotMatrix, kViewRotation);
  charge(kCosts.afterRotation);
  callGuestNow(c_, kOwner, kSetTransMatrix, kViewTranslation);
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    const std::int32_t offset = signedWord(object_ + kObjectPosition + axis * 4u) - signedWord(kEye + axis * 4u);
    c_.mem_w16(frame_ + kViewVector + axis * 2u, static_cast<std::uint16_t>(offset >> 8));
  }
  charge(kCosts.afterTranslation);
  callGuestNow(c_, kOwner, kRotTrans, frame_ + kViewVector, frame_ + kViewPosition, frame_ + kFar);
  holdBodyRegisters();
  charge(kCosts.levelFlag);
  const Verdict verdict = test();
  switch (verdict) {
  case Verdict::Kept:
    resume(kKept);
    return;
  case Verdict::Untested:
    resume(kUntested);
    return;
  case Verdict::Rejected:
    return;
  case Verdict::Margin:
    drawMargin();
    return;
  }
}

Verdict Head::test() {
  if ((word(kLevelFlags) & kUntestedLevel) != 0u) {
    return Verdict::Untested;
  }
  charge(kCosts.farLoad);
  const std::uint32_t zone = word(word(kCameraZone) + kZoneHeader);
  const std::int32_t far = signedWord(zone + kFarClip) >> 8;
  c_.mem_w32(frame_ + kFar, static_cast<std::uint32_t>(far));
  v1_ = static_cast<std::uint32_t>(far);
  const std::int32_t z = signedWord(frame_ + kViewPosition + 8u);
  if (far != 0) {
    charge(kCosts.farTest);
    if (far < z) {
      reject(kCosts.rejectJump);
      return Verdict::Rejected;
    }
    charge(kCosts.farKept);
  } else {
    charge(kCosts.defaultFarTest);
    if (z > kDefaultFar) {
      reject(0);
      return Verdict::Rejected;
    }
  }
  charge(kCosts.alwaysVisible);
  const std::uint32_t status = word(object_ + kObjectStatus);
  v1_ = status;
  if ((status & kAlwaysVisible) != 0u) {
    return Verdict::Kept;
  }
  charge(kCosts.nearTest);
  const std::int32_t h = signedWord(kProjection);
  if (h >= z) {
    reject(kCosts.rejectJump);
    return Verdict::Rejected;
  }
  charge(kCosts.kind);
  if (static_cast<std::int32_t>(status) < 0) {
    return boxTest();
  }
  return centreTest();
}

Verdict Head::boxTest() {
  charge(kCosts.boxCall - (entry_[kA3] != 0u ? 0u : kCosts.boxCallNoA3));
  callGuestNow(c_, kOwner, kProjectBound, object_ + kObjectBound, entry_[kA0] + 0x2Cu, object_ + kObjectPosition, kEye);
  holdBodyRegisters();
  const std::int32_t left = signedWord(kExtents);
  const std::int32_t top = signedWord(kExtents + 4u);
  const std::int32_t right = signedWord(kExtents + 8u);
  const std::int32_t bottom = signedWord(kExtents + 12u);
  v1_ = static_cast<std::uint32_t>(right);
  charge(kCosts.boxLoad);
  // The retail order: left of the screen, right of it, above, below.
  bool columns = false;
  if (left < -kRetailHalfWidth) {
    charge(kCosts.pairTest);
    if (right < -kRetailHalfWidth) {
      columns = true;
    }
  }
  if (!columns) {
    charge(kCosts.rightTest);
    if (left > kRetailHalfWidth) {
      charge(kCosts.rightTest);
      if (right > kRetailHalfWidth) {
        columns = true;
      }
    }
  }
  bool rows = false;
  std::uint32_t rowsCost = 0;
  if (!columns) {
    rowsCost += kCosts.pairTest;
    if (top < kRowsAbove) {
      rowsCost += kCosts.pairTest;
      rows = bottom < kRowsAbove;
    }
    if (!rows) {
      rowsCost += kCosts.rowsBelowFirst;
      if (top > kRowsBelow) {
        rowsCost += kCosts.rowsBelowSecond;
        if (bottom > kRowsBelow) {
          rowsCost += kCosts.rowsRejected;
          rows = true;
        }
      }
    }
    charge(rowsCost);
    if (!rows) {
      return Verdict::Kept;
    }
    reject(0);
    return Verdict::Rejected;
  }
  // A column reject from the left edge falls straight to the epilogue; from the right it jumps there.
  reject(left < -kRetailHalfWidth ? 0u : kCosts.rejectJump);
  const bool canvasRows = !(top < kRowsAbove && bottom < kRowsAbove) && !(top > kRowsBelow && bottom > kRowsBelow);
  const bool canvasColumns =
      !(left < canvas_.left && right < canvas_.left) && !(left > canvas_.right && right > canvas_.right);
  return canvasRows && canvasColumns ? Verdict::Margin : Verdict::Rejected;
}

Verdict Head::centreTest() {
  const std::int32_t z = signedWord(frame_ + kViewPosition + 8u);
  const std::int32_t h = signedWord(kProjection);
  if (z == 0 || z == -1) {
    lucent::error("crash1-object-cull", "object 0x{:08X} at depth {} traps the retail divide", object_, z);
    std::abort();
  }
  const auto project = [&](std::uint32_t axis) {
    const std::uint32_t product =
        static_cast<std::uint32_t>(signedWord(frame_ + kViewPosition + axis * 4u)) * static_cast<std::uint32_t>(h);
    v1_ = product;
    return static_cast<std::int32_t>(product) / z;
  };
  const std::int32_t sx = project(0);
  const std::int32_t sy = project(1);
  charge(kCosts.centreTest + (sx < 0 ? kCosts.centreNegative : 0u));
  const std::int32_t width = sx < 0 ? -sx : sx;
  if (width >= kRetailHalfWidth + kCentreSlack) {
    reject(kCosts.rejectJump);
    const bool canvasColumns = sx > canvas_.left - kCentreSlack && sx < canvas_.right + kCentreSlack;
    const bool rows = static_cast<std::uint32_t>(sy) + kCentreRowsBias < kCentreRows;
    return canvasColumns && rows ? Verdict::Margin : Verdict::Rejected;
  }
  charge(kCosts.centreRows);
  if (static_cast<std::uint32_t>(sy) + kCentreRowsBias >= kCentreRows) {
    reject(0);
    return Verdict::Rejected;
  }
  return Verdict::Kept;
}

// The retail epilogue: the saved registers back, 0 returned.
void Head::reject(std::uint32_t cost) {
  charge(cost + kCosts.epilogue);
  std::copy(entry_.begin(), entry_.end(), c_.r);
  c_.r[kV0] = 0u;
  c_.r[kV1] = v1_;
}

void Head::resume(std::uint32_t pc) {
  const std::optional<ImageIdentity> image = c_.imageCatalog().resolve(kEntry);
  if (!image) {
    lucent::error("crash1-object-cull", "no code image owns 0x{:08X}", kEntry);
    std::abort();
  }
  holdBodyRegisters();
  completeOrPropagate(c_,
                      resumeOriginal(c_, NativeKey{*image, kEntry}, pc, entry_[kRa], ExecutionBudget::currentTurn(c_)));
}

std::uint32_t primsBytes(Core &core) {
  switch (core.mem_r32(kLevel)) {
  case 0x19u:
    return kPrimsBytes19;
  case 0x38u:
    return kPrimsBytes38;
  default:
    return kPrimsBytes;
  }
}

// The object's matrices and its caller's draw, run as the retail keep would run them; only the packets, their
// ordering-table links and the primitive tail outlive the run.
void Head::drawMargin() {
  const std::uint32_t site = entry_[kRa];
  const Caller *caller = nullptr;
  for (const Caller &known : kCallers) {
    if (known.site == site) {
      caller = &known;
    }
  }
  if (caller == nullptr) {
    lucent::error("crash1-object-cull", "Gfx_CalcObjectMatrices called from an unknown site 0x{:08X}", site);
    std::abort();
  }
  const std::uint32_t callerReturn = word(entry_[kSp] + caller->savedRa);
  const std::uint32_t context = word(kContext);
  const std::uint32_t tail = word(context + kPrimsTail);
  const std::uint32_t end = word(context + kPrimsHead) + primsBytes(c_);
  psx::present::GuestRedraw redraw(c_);
  redraw.keep(context + kOt, kOtBytes);
  redraw.keep(context + kPrimsTail, 4u);
  if (tail < end) {
    redraw.keep(tail, end - tail);
  }
  holdBodyRegisters();
  if (!redraw.resumeTo(kEntry, kKept, callerReturn)) {
    redraw.keepNothing();
    return;
  }
  if (word(context + kPrimsTail) > end) {
    lucent::warn("crash1-object-cull", "object 0x{:08X} in the margin overflows the primitive buffer", object_);
    redraw.keepNothing();
  }
}

void calcObjectMatrices(Core *core) {
  Head(*core).run();
}

} // namespace

void ObjectCull::registerOverride(Core &core) {
  installNativeOverride(core, kEntry, kOwner, calcObjectMatrices, NativeReturn::Value);
}

} // namespace crash1
