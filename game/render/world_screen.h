// world_screen.h — what every Crash world drawer shares (crash2-decomp and crash3-decomp docs/notes/screen-culls.md):
// the retail packed screen test, its columns widened to the presented canvas, and the ordering table's head link.
#pragma once

#include "screen_cull.h"

#include <cstdint>

class Core;

namespace crash::world {

// The retail screen: 512 columns, 217 rows, packed as the drawers subtract it.
inline constexpr std::uint32_t kScreenEnd = 0x00D90200u;
inline constexpr int kScreenColumns = 0x200;

// Which edge rejected a polygon: the rows' (bit 31, tested first) or the columns' (bit 15).
enum class Cull : std::uint8_t {
  Visible,
  Rows,
  Columns,
};

struct ScreenVerdict {
  Cull cull = Cull::Visible;
  // Kept only because the canvas is wider than the retail screen.
  bool margin = false;
};

// Off the screen: every corner left of the first column, right of the last, or past the last row. The rows and the
// retail columns are the packed test; a wider canvas moves only the column edges.
class ScreenTest {
public:
  explicit ScreenTest(psx::present::CullWindow columns) : columns_(columns) {}

  ScreenVerdict test(const std::uint32_t *screens, int corners) const;
  bool widened() const {
    return columns_.left != 0 || columns_.right != kScreenColumns;
  }

private:
  psx::present::CullWindow columns_;
};

// The primitive at `next`, `words` long, at the head of the ordering table slot at `slot`; `next` moves past it.
void linkAtHead(Core &core, std::uint32_t slot, std::uint32_t &next, std::uint32_t words);

} // namespace crash::world
