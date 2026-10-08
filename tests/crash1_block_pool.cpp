// Falsifiers for the block-pool owner: the recovered walk against a Core-free fixture, and the owner
// through the real runtime (which bucket it read, what it published, what it returns for a bucket outside
// main RAM). Inputs are recorded in titles/crash1/executable.json.

#include "crash1_block_pool.h"

#include "core.h"
#include "crash1_runtime.h"
#include "game.h"
#include "game_runtime.h"
#include "hw_bind.h"
#include "image_identity.h"
#include "native_dispatch.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using crash1::BlockCell;
using crash1::CellSearch;
using crash1::Crash1BlockPool;

int failures = 0;

void check(bool condition, const char *what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

// A sparse bucket fixture inside main RAM that answers `kNoCell` for undeclared cells, counts reads
// outside main RAM instead of answering them, and records every address read.
class Fixture {
public:
  explicit Fixture(std::vector<std::uint32_t> classes) : classes_(std::move(classes)) {}

  static constexpr std::uint32_t kNoCell = 0xFFFFFFFFu;
  static constexpr std::uint32_t kBase = 0x80100000u;

  std::uint32_t base() const {
    return kBase;
  }
  std::uint32_t size() const {
    return static_cast<std::uint32_t>(classes_.size());
  }
  std::uint32_t classAt(std::uint32_t index) const {
    return classes_.at(index);
  }
  std::uint32_t reads() const {
    return reads_;
  }
  // Cells the walk can step before leaving main RAM, from kBase in 8-byte cells.
  std::uint32_t cellsToMainRamEdge() const {
    return (0x80200000u - kBase) / crash1::kCellStride;
  }
  std::uint32_t lastAddress() const {
    return lastAddress_;
  }
  std::uint32_t askedOutsideMainRam() const {
    return askedOutsideMainRam_;
  }

  static std::uint32_t read(const void *context, std::uint32_t cell) {
    // The reader gets a `const void *`; the const is dropped here, as in the owner.
    auto &self = *const_cast<Fixture *>(static_cast<const Fixture *>(context));
    ++self.reads_;
    self.lastAddress_ = cell;
    if (!crash1::isMainRam(cell)) {
      // Counted, not answered: answering would be the failure.
      ++self.askedOutsideMainRam_;
      return kNoCell;
    }
    const std::uint32_t offset = cell - kBase;
    if (offset % crash1::kCellStride != 0u) {
      std::fprintf(stderr, "FAIL: the walk read 0x%08X, which is not on the 8-byte cell grid\n", cell);
      return kNoCell;
    }
    const std::uint32_t index = offset / crash1::kCellStride;
    return index < self.classes_.size() ? self.classes_[index] : kNoCell;
  }

private:
  std::vector<std::uint32_t> classes_;
  std::uint32_t reads_{};
  std::uint32_t lastAddress_{};
  std::uint32_t askedOutsideMainRam_{};
};

constexpr std::uint32_t kNearPlaneConsumer = 0x8003A144u;
constexpr std::uint32_t kFaultSite = 0x800159A8u;

} // namespace

int main() {
  // The recovered walk.
  //
  // 0x80015994 tests the first cell before the loop: a first-cell match takes no step (`cellsWalked` 0).
  {
    const Fixture fixture({7u, 9u, 11u});
    const CellSearch search = crash1::searchCells(&fixture, &Fixture::read, fixture.base(), 7u);
    check(!search.leftMainRam, "a class on the first cell is found, not left main RAM");
    check(search.foundCell == fixture.base(), "the FIRST cell is returned, as 0x8001599C does");
    check(search.cellsWalked == 0u, "a first-cell match walks no cell: 0 of the array is examined");
    check(fixture.reads() == 1u, "exactly one class read happened, and it was the first cell");
    check(fixture.askedOutsideMainRam() == 0u, "no read left main RAM");
  }

  // A later match returns the matching cell, not its successor (the `+8` delay slot and `-8` at 0x800159B8);
  // the successor carries a different class.
  {
    const Fixture fixture({7u, 9u, 11u});
    const CellSearch search = crash1::searchCells(&fixture, &Fixture::read, fixture.base(), 11u);
    check(!search.leftMainRam, "a class on the third cell is found");
    check(search.foundCell == fixture.base() + 2u * crash1::kCellStride,
          "the cell that MATCHED is returned, not the one after it (0x800159B8 undoes the delay slot)");
    check(search.cellsWalked == 2u, "two stride steps were taken before the match: 2 cells walked");
    check(fixture.reads() == 3u, "the first cell and both successors were read, once each");
  }

  // 0x8001599C is `beq $2,$4`: the class is the whole request, and the shift only picks the bucket. A
  // cell class carrying the bits below the shift is servable only by an owner comparing the whole word.
  {
    constexpr std::uint32_t kRequest = 0x0057CCFBu; // a real request word measured from the live run
    const std::uint32_t shifted = kRequest >> crash1::kClassShift;
    check(shifted != kRequest && (kRequest & ((1u << crash1::kClassShift) - 1u)) != 0u,
          "the fixture request actually carries bits below the shift, so the two models differ");
    const Fixture fixture({kRequest});
    const CellSearch search = crash1::searchCells(&fixture, &Fixture::read, fixture.base(), kRequest);
    check(!search.leftMainRam && search.foundCell == fixture.base() && search.cellsWalked == 0u,
          "a cell whose class IS the whole request is found on the first cell; the shift is a bucket "
          "selector, not the class");
    const Fixture missFixture({shifted});
    const CellSearch miss = crash1::searchCells(&missFixture, &Fixture::read, missFixture.base(), kRequest);
    check(miss.leftMainRam && miss.foundCell == 0x80200000u,
          "and a cell carrying only the SHIFTED key is not a match, which is exactly what the "
          "pre-correction owner looked for and never found");
  }

  // An unserved class runs to the edge of main RAM and stops without reading past it.
  {
    const Fixture fixture({7u, 9u, 11u});
    const CellSearch search = crash1::searchCells(&fixture, &Fixture::read, fixture.base(), 99u);
    check(search.leftMainRam, "an absent class leaves main RAM rather than returning a neighbour");
    check(fixture.askedOutsideMainRam() == 0u, "the walk did NOT read outside main RAM: the edge held");
    // The fixture sits at 0x80100000; the first cell address at or past 0x80200000 stops the walk.
    check(search.cellsWalked == fixture.cellsToMainRamEdge(),
          "the walk stepped once per 8-byte cell to the edge of main RAM and no further");
    check(search.foundCell == 0x80200000u, "and it stopped AT the edge, 0x80200000");
    check(fixture.lastAddress() == 0x801FFFF8u,
          "and the last cell it actually READ is the last one inside main RAM, 0x801FFFF8");
  }

  // Transparency: for every class the fixture serves, the cell, read count and step count match the guest's.
  {
    for (std::uint32_t classKey : {4u, 8u, 12u}) {
      // Fresh fixture per class: the read counter is cumulative.
      const Fixture fixture({4u, 8u, 4u, 12u, 8u, 12u});
      const std::uint32_t expectedIndex = [&] {
        for (std::uint32_t i = 0; i < fixture.size(); ++i) {
          if (fixture.classAt(i) == classKey) {
            return i;
          }
        }
        return fixture.size();
      }();
      const CellSearch search = crash1::searchCells(&fixture, &Fixture::read, fixture.base(), classKey);
      check(!search.leftMainRam, "a served class never leaves main RAM");
      check(search.foundCell == fixture.base() + expectedIndex * crash1::kCellStride,
            "the owner returns the cell the guest's own walk would return, for a served class");
      check(search.cellsWalked == expectedIndex, "and it took exactly as many steps as the guest would");
      check(fixture.reads() == expectedIndex + 1u, "and it read exactly the cells the guest would read");
    }
  }

  // The bucket offset is `((request >> 13) & 0x3FC)` bytes.
  check((0x4000u >> crash1::kClassShift) == 2u, "one 8 KiB class of request is bucket key 2");
  check((0x1FFFu >> crash1::kClassShift) == 0u, "the largest request below 8 KiB is bucket key 0");
  check(((0x0057CCFBu >> crash1::kClassShift) & crash1::kBucketIndexMask) == 700u,
        "the request the live run measured, 0x0057CCFB, indexes bucket byte offset 700");
  check((4u & crash1::kBucketIndexMask) == 4u, "bucket key 4 sits at byte offset 4, one word in");
  check((crash1::kBucketIndexMask / 4u) == 255u,
        "the byte mask spans 256 buckets of 4 bytes, which is the table the guest indexes");
  // Live requests include unaligned bucket offsets: 0x0057CCFB -> 700, 0x15814CE7 -> 8, 0x5452D94D -> 660,
  // 0x4E938CCD -> 156. psxport's word accessor tolerates them; what a retail CPU does is not established.
  check(!crash1::isWordAlignedClassKey(0x0057CCFBu),
        "0x0057CCFB, a request the live run measured, indexes an UNALIGNED bucket — so the former "
        "'only multiples of four' claim is false and this measurement replaces it");
  check(crash1::isWordAlignedClassKey(0x4E938CCDu) && !crash1::isWordAlignedClassKey(0x00002000u),
        "alignment is a property of the request's bits 13 and up, and requests differ on it: "
        "0x00002000 shifts to 1 (odd, unaligned) and 0x4E938CCD shifts to 19660 (even, aligned)");
  check(crash1::kClassShift == 13u, "the shift that indexes the bucket is 13, the `sa` of 0x00041342");
  check(crash1::kCellStride == 8u && crash1::kClassFieldOffset == 4u,
        "the cell is 8 bytes and its class is the second word, from 0x24630008 and 0x8C620004");
  check(crash1::isMainRam(0x80100000u), "a real cell address IS main RAM");
  check(!crash1::isMainRam(0x80200000u), "the first cell address past main RAM is not");
  check(!crash1::isMainRam(0x00800000u), "0x00800000 is not main RAM, which is the whole claim");
  check(!crash1::isMainRam(0x00000000u),
        "a null bucket is refused: a null test alone would call this a pointer, and 0 is mapped here");
  check(!crash1::isMainRam(0x1F802000u),
        "an expansion-region address is refused too, so the test is a range test and not a null test");

  // The owner, through the real runtime.
  crash1::Crash1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  if (game->core.runtime != &runtime) {
    std::fprintf(stderr, "FAIL: the runtime did not install onto the Core\n");
    return 1;
  }
  // Activate the image first: an override key is (image identity, guest address).
  const auto identity = game->core.imageCatalog().activate("Crash1BlockPool", {0x00010000u, 0x00056800u}, 1u);
  crash1::installCrash1BlockPool(game->core);

  check(game->core.nativeDispatcher().isInstalled({identity, crash1::kFindCell}),
        "the owner installed its override on the measured lookup entry 0x80015978");
  check(!game->core.nativeDispatcher().isInstalled({identity, kNearPlaneConsumer}),
        "the GTE near-plane consumer is not overwritten: this module owns a pool, not a projection");
  check(!game->core.nativeDispatcher().isInstalled({identity, kFaultSite}),
        "the stop address 0x800159A8 is INSIDE the lookup, so it is an arm of the owner's loop and "
        "never an override key of its own; installing one would resume at a label the guest reaches "
        "by falling through, with no r31 set by the jump that got there");

  Crash1BlockPool &owner = runtime.blockPool();
  check(owner.lookups() == 0u && owner.found() == 0u,
        "0 of 0 lookups before the owner has run: a denominator, not a result");

  // A live pool published through the three guest globals, with whole 32-bit request words carrying bits
  // below the shift.
  const std::uint32_t kBucketTable = 0x80060000u;
  const std::uint32_t kCellCountBlock = 0x80060010u;
  const std::uint32_t kFirstCell = 0x80100000u;
  const std::uint32_t kRequestServed = 0x0057CCFBu;     // bucket byte offset 700
  const std::uint32_t kRequestAlsoServed = 0x5452D94Du; // bucket byte offset 660
  const std::uint32_t kRequestUnmapped = 0x15814CE7u;   // bucket byte offset 8
  game->core.mem_w32(crash1::kBucketTablePointer, kBucketTable);
  game->core.mem_w32(crash1::kPoolBasePointer, kFirstCell);
  game->core.mem_w32(crash1::kCellCountBlockPointer, kCellCountBlock);
  game->core.mem_w32(kCellCountBlock + crash1::kCellCountOffset, 3u);
  for (std::uint32_t bucket = 0; bucket < 256u; ++bucket) {
    game->core.mem_w32(kBucketTable + bucket * 4u, kFirstCell);
  }
  game->core.mem_w32(kFirstCell + 0u, 0x00001234u);         // payload of cell 0
  game->core.mem_w32(kFirstCell + 4u, kRequestServed);      // class of cell 0
  game->core.mem_w32(kFirstCell + 8u, 0x00005678u);         // payload of cell 1
  game->core.mem_w32(kFirstCell + 12u, kRequestAlsoServed); // class of cell 1
  game->core.mem_w32(kFirstCell + 16u, 0x00009ABCu);        // payload of cell 2
  game->core.mem_w32(kFirstCell + 20u, kRequestServed);     // class of cell 2

  // A request the FIRST cell serves: the answer is the first cell and no step is taken.
  game->core.r[31] = 0x80013140u; // the return address the first measured call site would carry
  check(owner.findCell(game->core, kRequestServed) == kFirstCell,
        "a request whose WHOLE word is cell 0's class returns that cell — the low 13 bits are part "
        "of the class, exactly as `beq $2,$4` at 0x8001599C compares them");
  check(owner.lookups() == 1u && owner.found() == 1u, "1 of 1 lookups found a cell");
  check(owner.firstCaller() == 0x80013140u,
        "the caller's return address is recorded once, so a run can name the module that asked");
  check(owner.lastClassKey() == 700u,
        "the recorded bucket offset is (request>>13)&0x3FC = 700, and it is NOT the class");
  check(owner.lastRequest() == kRequestServed, "the request the guest passed is recorded");
  check(owner.lastBucketTable() == kBucketTable, "the bucket table the guest published is recorded");
  check(owner.lastFirstCell() == kFirstCell, "the bucket's first cell is recorded");
  check(owner.lastPoolBase() == kFirstCell, "the engine's pool base is recorded");
  check(owner.lastLiveCellCount() == 3u, "the engine's live cell count is recorded");
  check(owner.lastPoolBaseDistance() == 0,
        "the signed distance from the engine's base is reported, and is 0 for a bucket sitting on it");

  // The same request now served by the third cell; the pool is mutated so a cached answer would fail.
  game->core.mem_w32(kFirstCell + 4u, kRequestAlsoServed);
  game->core.mem_w32(kFirstCell + 12u, kRequestAlsoServed);
  check(owner.findCell(game->core, kRequestServed) == kFirstCell + 2u * crash1::kCellStride,
        "a class on the third cell returns the third cell, walking two steps");
  check(owner.lastCellsWalked() == 2u, "the walk examined 2 cells after the first");
  check(owner.maxCellsWalked() == 2u, "the deepest walk so far is 2 cells");
  check(owner.firstCaller() == 0x80013140u,
        "a second call from elsewhere does not overwrite the first caller: the one sample is the point");
  check(owner.lookups() == 2u && owner.found() == 2u, "2 of 2 lookups found a cell");

  // A bucket that is not a pointer into main RAM: the owner must not read through it and must name the value.
  const std::uint32_t unmappedOffset = (kRequestUnmapped >> crash1::kClassShift) & crash1::kBucketIndexMask;
  game->core.mem_w32(kBucketTable + unmappedOffset, 0x00800000u);
  const std::uint32_t unmapped = owner.findCell(game->core, kRequestUnmapped);
  check(unmapped == 0x00800000u,
        "the owner returns the pointer it was given rather than inventing a failure value: the "
        "guest's callers dereference the result unchecked, so a substituted error word would fault "
        "one level up and hide the real value");
  check(owner.leftMainRam() == 1u, "the walk that left main RAM is counted");
  check(owner.found() == 2u,
        "and it is NOT counted as a lookup that found a cell: those are different outcomes and one "
        "denominator cannot stand for both");
  check(owner.lookups() == 3u, "3 lookups served, 2 found a cell, 1 left main RAM");
  check(owner.lastCellsWalked() == 0u,
        "the walk that left main RAM read no cell at all, so it walked none: the count is 0 because "
        "the FIRST address was already outside, which is the whole shape of the media-less fault");
  check(owner.maxCellsWalked() == 2u,
        "and the running maximum is still the deepest walk that actually happened, which is the "
        "number a reader wants beside it");
  check(owner.lastFirstCell() == 0x00800000u, "and the offending pointer is the recorded first cell");

  std::printf("Crash1BlockPool: %llu lookup(s) served, %llu found a cell, %llu left main RAM; "
              "deepest walk %u cell(s); first caller 0x%08X; the recovered walk is transparent for "
              "every served class and stops only at the edge of main RAM\n",
              static_cast<unsigned long long>(owner.lookups()),
              static_cast<unsigned long long>(owner.found()),
              static_cast<unsigned long long>(owner.leftMainRam()),
              owner.maxCellsWalked(),
              owner.firstCaller());
  if (failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}
