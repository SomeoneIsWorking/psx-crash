#pragma once

#include "drawn_logo.h"
#include "gool/gool_object.h"

#include <cstdint>
#include <optional>
#include <span>

class Core;

namespace crash {

// Objects of one executable and subtype that each stand at one code word while idle.
struct GoolHold {
  std::uint32_t executableEid = 0;
  std::uint32_t subtype = 0;
  std::uint32_t codeWord = 0;
};

// The title screen's own logo: the GOOL objects whose states say the wordmark stands finished, and the
// textures it is drawn from. None yet: the letters are untextured models (issue 0036).
struct TitleLogoFacts {
  GoolObjectLayout layout;
  std::uint32_t playerObject = 0; // guest word holding the player object, the title screen's root
  GoolObjectKey screen;           // the player or a descendant in this state while the finished logo stands
  std::optional<GoolHold> pieces; // the logo's own pieces, every one idle once it has landed
  std::span<const psx::present::LogoTexture> textures;
};

// Whether the title screen shows its finished logo right now.
bool titleLogoUp(Core &core, const TitleLogoFacts &facts);

} // namespace crash
