// Input of the native Mega Drive (spec 008, TP-108.3): two pads of twelve
// controls each, stored port-major in an InputState. The control order is the
// API's, the one of ClownMDEmu, FX-01 and the traces: up, down, left, right,
// A, B, C, start, then X, Y, Z, mode (TP-112.2). pad_mask turns it into the
// bit order of System::set_pad (B, C, A at bits 4, 5, 6).
#pragma once
#include "md/system.hpp"
#include "systems/emulated_system.hpp"

#include <cstdint>
#include <optional>

namespace tessera::md {

/// Pressed mask of `port` in `input`, or nothing when the pad is
/// contradictory: up with down, or left with right, which no pad can produce
/// (TESSERA_INPUT_CONTRADICTORY).
inline std::optional<std::uint16_t> pad_mask(const InputState &input,
                                             unsigned port) noexcept {
  static constexpr std::uint16_t kControlBits[kPadControls] = {
      kPadUp, kPadDown,  kPadLeft, kPadRight, kPadA, kPadB,
      kPadC,  kPadStart, kPadX,    kPadY,     kPadZ, kPadMode};
  const auto *c = input.data() + std::size_t{port} * kPadControls;
  if ((c[0] && c[1]) || (c[2] && c[3]))
    return std::nullopt;
  std::uint16_t mask = 0;
  for (unsigned control = 0; control < kPadControls; ++control)
    if (c[control])
      mask = static_cast<std::uint16_t>(mask | kControlBits[control]);
  return mask;
}

/// Hands both pads to the machine. False, and nothing applied, if either pad
/// is contradictory.
inline bool apply_input(System &system, const InputState &input) noexcept {
  const auto first = pad_mask(input, 0);
  const auto second = pad_mask(input, 1);
  if (!first || !second)
    return false;
  system.set_pad(0, *first);
  system.set_pad(1, *second);
  return true;
}

} // namespace tessera::md
