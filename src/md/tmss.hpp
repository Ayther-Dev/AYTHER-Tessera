// Optional TMSS of the native Mega Drive core (spec 008, TP-101.5).
// With TMSS enabled the console only boots a cartridge whose header holds
// "SEGA" ($100, or $101 for " SEGA"), and the VDP stays locked until the
// program writes the ASCII word "SEGA" to $A14000.
#pragma once
#include "md/machine.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace tessera::md {

/// True when the header of `rom` carries "SEGA" at $100 or $101.
bool header_has_sega(std::span<const std::uint8_t> rom) noexcept;

/// With TMSS a ROM without "SEGA" does not boot; without TMSS any ROM does.
bool can_boot(const MachineConfig &config,
              std::span<const std::uint8_t> rom) noexcept;

class Tmss {
public:
  explicit Tmss(bool enabled) noexcept : enabled_(enabled) {}

  /// Write to the 4 byte register; `offset` is 0..3, `word` for 16 bit writes.
  void write(std::uint32_t offset, std::uint16_t value, bool word) noexcept;
  bool locked() const noexcept { return enabled_ && !unlocked_; }
  void reset() noexcept;

private:
  bool enabled_;
  bool unlocked_ = false;
  std::array<std::uint8_t, 4> reg_{};
};

} // namespace tessera::md
