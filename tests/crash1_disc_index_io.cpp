#include "crash1_disc_index_io.h"

#include "core.h"
#include "game.h"
#include "testutil.h"

#include <memory>

namespace {

enum { V0 = 2, A0 = 4, A1 = 5, A2 = 6 };

constexpr std::uint32_t kPosition = 0x80110000u;
constexpr std::uint32_t kReadBuffer = 0x80111000u;
constexpr std::uint32_t kResult = 0x80112000u;

void writePosition(Core &core, std::uint8_t minute, std::uint8_t second, std::uint8_t frame) {
  core.mem_w8(kPosition, minute);
  core.mem_w8(kPosition + 1u, second);
  core.mem_w8(kPosition + 2u, frame);
  core.mem_w8(kPosition + 3u, 0u);
}

void fillResult(Core &core) {
  for (std::uint32_t i = 0; i < 8u; ++i) {
    core.mem_w8(kResult + i, static_cast<std::uint8_t>(0xA0u + i));
  }
}

void checkResultCleared(Core &core) {
  for (std::uint32_t i = 0; i < 8u; ++i) {
    CHECK_EQ(core.mem_r8(kResult + i), 0u);
  }
}

} // namespace

static void test_setloc_routes_through_shipping_control_owner() {
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  writePosition(core, 0x00u, 0x02u, 0x10u);
  fillResult(core);
  core.r[A0] = 0x02u;
  core.r[A1] = kPosition;
  core.r[A2] = kResult;

  crash1::disc_index_io::applyControl(&core);

  CHECK_EQ(core.r[V0], 1u);
  CHECK_EQ(game->cd.setloc_lba, 10);
  checkResultCleared(core);
}

static void test_setloc_f_routes_without_a_result_pointer() {
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  writePosition(core, 0x11u, 0x27u, 0x01u);
  fillResult(core);
  core.r[A0] = 0x02u;
  core.r[A1] = kPosition;
  core.r[A2] = kResult;

  crash1::disc_index_io::applyControlF(&core);

  CHECK_EQ(core.r[V0], 1u);
  CHECK_EQ(game->cd.setloc_lba, 51376);
  CHECK_EQ(core.r[A2], kResult);
  for (std::uint32_t i = 0; i < 8u; ++i) {
    CHECK_EQ(core.mem_r8(kResult + i), static_cast<std::uint8_t>(0xA0u + i));
  }
}

static void test_zero_sector_read_completes_without_drive_work() {
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  game->cd.setloc_lba = 10;
  game->cd.sec_pos = 24;
  game->cd.sec_len = 2352;
  game->cd.sec_lba = 9;
  game->cd.stock_reading = 1;
  core.r[A0] = 0u;
  core.r[A1] = kReadBuffer;
  core.r[A2] = 0u;

  crash1::disc_index_io::applyRead(&core);

  CHECK_EQ(core.r[V0], 1u);
  CHECK_EQ(game->cd.setloc_lba, 10);
  CHECK_EQ(game->cd.sec_pos, 0);
  CHECK_EQ(game->cd.sec_len, 0);
  CHECK_EQ(game->cd.sec_lba, -1);
  CHECK_EQ(game->cd.stock_reading, 0);
}

static void test_cd_sync_reports_complete_without_display_timing() {
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  fillResult(core);
  core.r[A0] = 1u;
  core.r[A1] = kResult;

  crash1::disc_index_io::applySync(&core);

  CHECK_EQ(core.r[V0], 2u);
  checkResultCleared(core);
  CHECK_EQ(game->timing.vblank, 0u);
}

static void test_read_sync_reports_no_remaining_sectors() {
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  fillResult(core);
  core.r[A0] = 0u;
  core.r[A1] = kResult;

  crash1::disc_index_io::applyReadSync(&core);

  CHECK_EQ(core.r[V0], 0u);
  checkResultCleared(core);
}

int main() {
  RUN(setloc_routes_through_shipping_control_owner);
  RUN(setloc_f_routes_without_a_result_pointer);
  RUN(zero_sector_read_completes_without_drive_work);
  RUN(cd_sync_reports_complete_without_display_timing);
  RUN(read_sync_reports_no_remaining_sectors);
  return pt_summary();
}
