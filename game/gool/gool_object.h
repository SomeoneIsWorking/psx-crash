#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace crash {

// Where one title's GOOL object keeps the fields read from the host, from the object's own base.
struct GoolObjectLayout {
  std::uint32_t executable = 0; // the GOOL executable entry pointer
  std::uint32_t state = 0;      // current state index
  std::uint32_t subtype = 0;    // entity subtype; Crash 3 keeps the executable's type in the high half
  std::uint32_t pc = 0;         // the interpreter's program counter
  std::uint32_t child = 0;      // first child link
  std::uint32_t sibling = 0;    // next sibling link
};

// Crash 2 and Crash 3 objects, measured in both titles' title screens.
inline constexpr GoolObjectLayout kCrash2EngineObjectLayout{
    .executable = 0x0Cu, .state = 0x1Cu, .subtype = 0xB4u, .pc = 0xC0u, .child = 0x4Cu, .sibling = 0x48u};

// A GOOL executable's identity and one of its objects' (subtype, state) pairs.
struct GoolObjectKey {
  std::uint32_t executableEid = 0;
  std::uint32_t subtype = 0;
  std::uint32_t state = 0;
};

// A read-only view of one guest GOOL object.
class GoolObject {
public:
  GoolObject(Core &core, const GoolObjectLayout &layout, std::uint32_t address);

  // The object the guest word at `global` points at; nullopt when it holds no object.
  static std::optional<GoolObject> at(Core &core, const GoolObjectLayout &layout, std::uint32_t global);

  std::uint32_t executableEid() const;
  std::uint32_t subtype() const;
  std::uint32_t state() const;
  // The program counter as a word offset into the executable's code item.
  std::uint32_t codeWord() const;
  bool matches(const GoolObjectKey &key) const;

  std::optional<GoolObject> firstChild() const;
  std::optional<GoolObject> nextSibling() const;

private:
  std::optional<GoolObject> linked(std::uint32_t offset) const;
  std::uint32_t word(std::uint32_t offset) const;

  Core &core_;
  const GoolObjectLayout &layout_;
  std::uint32_t address_;
};

} // namespace crash
