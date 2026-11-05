// Input of the native Mega Drive (spec 008, TP-108.3): two pads of twelve
// controls each, stored port-major in an InputState (control order: up, down,
// left, right, B, C, A, start, X, Y, Z, mode), which is the bit order of the
// pad masks of System::set_pad.
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
  const auto *c = input.data() + std::size_t{port} * kPadControls;
  if ((c[0] && c[1]) || (c[2] && c[3]))
    return std::nullopt;
  std::uint16_t mask = 0;
  for (unsigned control = 0; control < kPadControls; ++control)
    if (c[control])
      mask = static_cast<std::uint16_t>(mask | (1U << control));
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
