#include "gool/gool_object.h"

#include "core.h"

namespace crash {
namespace {

// An NSF entry header: magic, EID, type, item count, then one pointer per item once the page is resident.
constexpr std::uint32_t kEntryEid = 0x4u;
constexpr std::uint32_t kCodeItem = 0x14u;
constexpr std::uint32_t kSubtypeMask = 0xFFFFu;

// Objects and resident entries live in KSEG0 main RAM; anything else is not an object.
bool inMainRam(std::uint32_t address) {
  return address >= 0x80000000u && address < 0x80200000u && (address & 3u) == 0;
}

} // namespace

GoolObject::GoolObject(Core &core, const GoolObjectLayout &layout, std::uint32_t address)
    : core_(core), layout_(layout), address_(address) {}

std::optional<GoolObject> GoolObject::at(Core &core, const GoolObjectLayout &layout, std::uint32_t global) {
  const std::uint32_t address = core.mem_r32(global);
  if (!inMainRam(address)) {
    return std::nullopt;
  }
  return GoolObject(core, layout, address);
}

std::uint32_t GoolObject::word(std::uint32_t offset) const {
  return core_.mem_r32(address_ + offset);
}

std::uint32_t GoolObject::executableEid() const {
  const std::uint32_t entry = word(layout_.executable);
  return inMainRam(entry) ? core_.mem_r32(entry + kEntryEid) : 0;
}

std::uint32_t GoolObject::subtype() const {
  return word(layout_.subtype) & kSubtypeMask;
}

std::uint32_t GoolObject::state() const {
  return word(layout_.state);
}

std::uint32_t GoolObject::codeWord() const {
  const std::uint32_t entry = word(layout_.executable);
  if (!inMainRam(entry)) {
    return 0;
  }
  return (word(layout_.pc) - core_.mem_r32(entry + kCodeItem)) / 4;
}

bool GoolObject::matches(const GoolObjectKey &key) const {
  return executableEid() == key.executableEid && subtype() == key.subtype && state() == key.state;
}

std::optional<GoolObject> GoolObject::linked(std::uint32_t offset) const {
  const std::uint32_t address = word(offset);
  if (!inMainRam(address)) {
    return std::nullopt;
  }
  return GoolObject(core_, layout_, address);
}

std::optional<GoolObject> GoolObject::firstChild() const {
  return linked(layout_.child);
}

std::optional<GoolObject> GoolObject::nextSibling() const {
  return linked(layout_.sibling);
}

} // namespace crash
