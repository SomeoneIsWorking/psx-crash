#include "gool/title_logo.h"

#include <vector>

namespace crash {
namespace {

// More than any title's object pool holds, so a torn link during a level load ends the walk.
constexpr int kMaxObjects = 512;

} // namespace

bool titleLogoUp(Core &core, const TitleLogoFacts &facts) {
  const std::optional<GoolObject> player = GoolObject::at(core, facts.layout, facts.playerObject);
  if (!player) {
    return false;
  }
  bool screen = false;
  std::vector<GoolObject> pending{*player};
  for (int visited = 0; !pending.empty() && visited < kMaxObjects; ++visited) {
    const GoolObject object = pending.back();
    pending.pop_back();
    screen = screen || object.matches(facts.screen);
    if (facts.pieces && object.executableEid() == facts.pieces->executableEid &&
        object.subtype() == facts.pieces->subtype && object.codeWord() != facts.pieces->codeWord) {
      return false;
    }
    if (std::optional<GoolObject> sibling = object.nextSibling(); sibling && visited > 0) {
      pending.push_back(*sibling);
    }
    if (std::optional<GoolObject> child = object.firstChild()) {
      pending.push_back(*child);
    }
  }
  return screen;
}

} // namespace crash
