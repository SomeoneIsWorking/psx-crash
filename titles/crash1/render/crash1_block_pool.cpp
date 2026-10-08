#include "crash1_block_pool.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crash1 {
namespace {

std::uint32_t readGuestCellClass(const void *context, std::uint32_t cell) {
  // Core::mem_r32 is not const; the const is dropped here only.
  auto &core = *const_cast<Core *>(static_cast<const Core *>(context));
  return core.mem_r32(cell + kClassFieldOffset);
}

void findCellOverride(Core *core) {
  Crash1BlockPool &pool = Crash1BlockPool::from(*core);
  core->r[2] = pool.findCell(*core, core->r[4]);
}

} // namespace

CellSearch
searchCells(const void *context, const CellClassReader &read, std::uint32_t firstCell, std::uint32_t request) noexcept {
  CellSearch search{firstCell, 0u, false};
  // Both branches (0x8001599C, 0x800159B0) compare the class against $a0 itself; the first cell is read
  // unchecked at 0x80015994, so one outside RAM is returned unchanged and the guest faults as retail does.
  if (!isMainRam(firstCell)) {
    search.leftMainRam = true;
    return search;
  }
  if (read(context, firstCell) == request) {
    return search;
  }
  for (std::uint32_t cell = firstCell + kCellStride;; cell += kCellStride) {
    ++search.cellsWalked;
    // A cell outside main RAM is one no retail execution reaches.
    if (!isMainRam(cell)) {
      search.foundCell = cell;
      search.leftMainRam = true;
      return search;
    }
    if (read(context, cell) == request) {
      search.foundCell = cell;
      return search;
    }
  }
}

std::uint32_t Crash1BlockPool::findCell(Core &core, std::uint32_t request) {
  if (firstCaller_ == 0u) {
    firstCaller_ = core.r[31];
  }

  // 0x80015978..0x80015988: the bucket selector, a byte offset (hence mask 0x3FC), not the class.
  const std::uint32_t classKey = (request >> kClassShift) & kBucketIndexMask;
  const std::uint32_t bucketTable = core.mem_r32(kBucketTablePointer);
  ++lookups_;
  lastRequest_ = request;
  lastClassKey_ = classKey;
  lastBucketTable_ = bucketTable;
  lastPoolBase_ = core.mem_r32(kPoolBasePointer);
  lastLiveCellCount_ = core.mem_r32(core.mem_r32(kCellCountBlockPointer) + kCellCountOffset);

  // 0x8001598C: the bucket's first cell.
  const std::uint32_t firstCell = core.mem_r32(bucketTable + classKey);
  lastFirstCell_ = firstCell;
  lastCellsWalked_ = 0u;

  const CellSearch search = searchCells(&core, readGuestCellClass, firstCell, request);
  lastCellsWalked_ = search.cellsWalked;
  if (search.cellsWalked > maxCellsWalked_) {
    maxCellsWalked_ = search.cellsWalked;
  }
  // The engine's bound, reported and not enforced; signed (`sra` then `slt`).
  lastPoolBaseDistance_ = static_cast<std::int32_t>(firstCell - lastPoolBase_) / static_cast<std::int32_t>(kCellStride);

  if (search.leftMainRam) {
    ++leftMainRam_;
    lucent::error("crash1-pool",
                  "SCUS-949.00 block pool: the walk for the class of request 0x{:08X} (bucket byte "
                  "offset {}) left main RAM at 0x{:08X} after {} cell(s) from first cell 0x{:08X}; the "
                  "bucket table is 0x{:08X}, the engine's own base 0x{:08X} is {} cell(s) away and it "
                  "publishes {} live cell(s) — across {} lookups served, {} found a cell, {} left main "
                  "RAM, first caller 0x{:08X}",
                  request,
                  classKey,
                  search.foundCell,
                  search.cellsWalked,
                  firstCell,
                  bucketTable,
                  lastPoolBase_,
                  lastPoolBaseDistance_,
                  lastLiveCellCount_,
                  lookups_,
                  found_,
                  leftMainRam_,
                  firstCaller_);
    return search.foundCell;
  }
  if (!reportedFirstLookup_) {
    reportedFirstLookup_ = true;
    lucent::info("crash1-pool",
                 "SCUS-949.00 block pool: the first lookup of this run — request 0x{:08X} (bucket "
                 "byte offset {}), bucket table 0x{:08X}, first cell 0x{:08X}, engine base 0x{:08X} "
                 "publishing {} live cell(s); 1 of 1 lookups served so far, caller 0x{:08X}",
                 request,
                 classKey,
                 bucketTable,
                 firstCell,
                 lastPoolBase_,
                 lastLiveCellCount_,
                 firstCaller_);
  }
  ++found_;
  if (!reportedFirstFound_) {
    reportedFirstFound_ = true;
    lucent::info("crash1-pool",
                 "SCUS-949.00 block pool: the first lookup that found a cell — request 0x{:08X} "
                 "(bucket byte offset {}), cell 0x{:08X} after {} cell(s) walked, first cell of the "
                 "bucket 0x{:08X}; {}-of-{} lookups served have found a cell, {}-of-{} left main RAM, "
                 "caller 0x{:08X}",
                 request,
                 classKey,
                 search.foundCell,
                 search.cellsWalked,
                 firstCell,
                 found_,
                 lookups_,
                 leftMainRam_,
                 lookups_,
                 firstCaller_);
  }
  return search.foundCell;
}

Crash1BlockPool &Crash1BlockPool::from(Core &core) {
  if (!core.runtime) {
    lucent::error("crash1-pool", "SCUS-949.00 block-pool override ran without its title runtime");
    std::abort();
  }
  auto *const runtime = dynamic_cast<Crash1Runtime *>(const_cast<GameRuntime *>(core.runtime));
  if (!runtime) {
    lucent::error("crash1-pool", "SCUS-949.00 block-pool override reached another title's runtime");
    std::abort();
  }
  return runtime->blockPool();
}

void installCrash1BlockPool(Core &core) {
  psx::cpu::installNativeOverride(core, kFindCell, "Crash block-pool cell lookup", findCellOverride);
  lucent::info("crash1-pool",
               "block-pool owner installed: cell lookup 0x{:08X} (the stop site is 0x{:08X}); bucket "
               "table 0x{:08X}, pool base 0x{:08X}, live count at 0x{:08X}+0x{:X}; the walk is "
               "stopped at the edge of main RAM, NOT at the engine's own bound at 0x{:08X}, which was "
               "applied and measured to break a path retail completes; 6 measured call sites",
               kFindCell,
               kFaultSite,
               kBucketTablePointer,
               kPoolBasePointer,
               kCellCountBlockPointer,
               kCellCountOffset,
               kFindCellBounded);
}

} // namespace crash1
