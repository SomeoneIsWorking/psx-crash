// The cell lookup at 0x80015978: `cell = bucket[(request >> 13 & 0x3FC) >> 2]`, then
// `while (cell->class != request) cell += 1`; the class is the whole request word.
#pragma once

#include <cstddef>
#include <cstdint>

class Core;

namespace crash1 {

inline constexpr std::uint32_t kFindCell = 0x80015978u;
inline constexpr std::uint32_t kFindCellEnd = 0x800159C4u;
inline constexpr std::uint32_t kFindCellBounded = 0x800159C4u;
inline constexpr std::uint32_t kFindCellBoundedEnd = 0x80015A3Cu;

// Decoded from the instructions that produce them.
inline constexpr std::uint32_t kClassShift = 13u;          // srl v0,a0,13 at 0x80015978
inline constexpr std::uint32_t kBucketIndexMask = 0x03FCu; // andi v0,v0,0x03FC at 0x80015984
inline constexpr std::uint32_t kCellStride = 8u;           // addiu v1,v1,8 at 0x800159A4
inline constexpr std::uint32_t kClassFieldOffset = 4u;     // lw v0,4(v1) at 0x80015994

// The pool's globals, each reached by a `lui 0x8006` + `lw` pair, so no immediate carries the address.
inline constexpr std::uint32_t kBucketTablePointer = 0x8005C530u;    // -> 256 bucket cell pointers
inline constexpr std::uint32_t kPoolBasePointer = 0x8005C534u;       // -> a pool base, for the bound form
inline constexpr std::uint32_t kCellCountBlockPointer = 0x8005C540u; // -> a block, +0x404 is the count
inline constexpr std::uint32_t kCellCountOffset = 0x404u;

// The bounded sibling's "no cell" result (`addiu v0,zero,-10` at 0x80015A18); 0x80015174 dereferences it,
// so this owner never returns it.
inline constexpr std::int32_t kPoolExhausted = -10;

inline constexpr std::uint32_t kFaultSite = 0x800159A8u;

// One 8-byte pool cell; `payload` is stored by the allocate path at 0x80013020. A layout description
// only: guest bytes are read through `CellClassReader`.
struct BlockCell {
  std::uint32_t payload;
  std::uint32_t cellClass; // the request word the engine compares at 0x8001599C, unshifted
};
static_assert(sizeof(BlockCell) == kCellStride, "the measured cell stride is the recovered cell size");
static_assert(offsetof(BlockCell, cellClass) == kClassFieldOffset,
              "the measured class field is the recovered cell's second word");

// Reads one word out of a guest cell; injected so the walk needs no Core.
using CellClassReader = std::uint32_t (*)(const void *context, std::uint32_t cell);

// Result of the recovered walk; addresses and counts only.
struct CellSearch {
  std::uint32_t foundCell;   // the cell whose class matched; meaningless when `leftMainRam` is set
  std::uint32_t cellsWalked; // cells examined; the denominator a reader needs to judge a walk
  bool leftMainRam;          // the next cell was outside the guest's 2 MiB of main RAM
};

// 0x80015994 tests the first cell, 0x800159A8 is the loop head; the walk stops when a cell address
// leaves main RAM.
[[nodiscard]] CellSearch
searchCells(const void *context, const CellClassReader &read, std::uint32_t firstCell, std::uint32_t request) noexcept;

// Main RAM is 2 MiB at 0x80000000, mirrored through KSEG0 and KSEG1.
[[nodiscard]] constexpr bool isMainRam(std::uint32_t address) noexcept {
  return (address & 0xFFE00000u) == 0x80000000u;
}

// The bucket offset indexes a word table, so bits 13 and up of the request must form a multiple of 4.
[[nodiscard]] constexpr bool isWordAlignedClassKey(std::uint32_t request) noexcept {
  return ((request >> kClassShift) & 0x3u) == 0u;
}

class Crash1BlockPool final {
public:
  // What the last lookup searched for and what the guest had published.
  [[nodiscard]] std::uint32_t lastRequest() const {
    return lastRequest_;
  }
  [[nodiscard]] std::uint32_t lastClassKey() const {
    return lastClassKey_;
  }
  [[nodiscard]] std::uint32_t lastBucketTable() const {
    return lastBucketTable_;
  }
  [[nodiscard]] std::uint32_t lastFirstCell() const {
    return lastFirstCell_;
  }
  [[nodiscard]] std::uint32_t lastPoolBase() const {
    return lastPoolBase_;
  }
  [[nodiscard]] std::uint32_t lastLiveCellCount() const {
    return lastLiveCellCount_;
  }
  [[nodiscard]] std::uint32_t lastCellsWalked() const {
    return lastCellsWalked_;
  }

  // The engine's bound, evaluated for the last lookup and reported, not enforced. Signed like the
  // guest's `sra`/`slt` pair: negative for a bucket below the base at 0x8005C534.
  [[nodiscard]] std::int32_t lastPoolBaseDistance() const {
    return lastPoolBaseDistance_;
  }

  // Denominators for any report of this owner.
  [[nodiscard]] std::uint64_t lookups() const {
    return lookups_;
  }
  [[nodiscard]] std::uint64_t found() const {
    return found_;
  }
  [[nodiscard]] std::uint64_t leftMainRam() const {
    return leftMainRam_;
  }
  [[nodiscard]] std::uint32_t maxCellsWalked() const {
    return maxCellsWalked_;
  }

  // Return address of the first lookup served; zero until one has run.
  [[nodiscard]] std::uint32_t firstCaller() const {
    return firstCaller_;
  }

  // Whether the first lookup and the first served cell have been reported.
  [[nodiscard]] bool reportedFirstLookup() const {
    return reportedFirstLookup_;
  }
  [[nodiscard]] bool reportedFirstFound() const {
    return reportedFirstFound_;
  }

  [[nodiscard]] std::uint32_t findCell(Core &core, std::uint32_t request);

  // This title's owner, reached from a Core running it.
  static Crash1BlockPool &from(Core &core);

private:
  std::uint32_t lastRequest_{};
  std::uint32_t lastClassKey_{};
  std::uint32_t lastBucketTable_{};
  std::uint32_t lastFirstCell_{};
  std::uint32_t lastPoolBase_{};
  std::uint32_t lastLiveCellCount_{};
  std::uint32_t lastCellsWalked_{};
  std::int32_t lastPoolBaseDistance_{};
  std::uint32_t maxCellsWalked_{};
  std::uint32_t firstCaller_{};
  bool reportedFirstLookup_{};
  bool reportedFirstFound_{};
  std::uint64_t lookups_{};
  std::uint64_t found_{};
  std::uint64_t leftMainRam_{};
};

void installCrash1BlockPool(Core &core);

} // namespace crash1
