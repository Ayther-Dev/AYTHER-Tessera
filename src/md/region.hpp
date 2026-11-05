// Console region of the native Mega Drive core (spec 008, TP-108.4, PQ-13).
//
// Clean-room origin: the header rule below was written from the public Sega
// header format (the country field at $1F0: the letters J, U and E, or a hex
// digit whose bits are 1 = Japan NTSC, 2 = Japan PAL, 4 = Americas,
// 8 = Europe) and then adjusted only from black-box observations of gpgx
// (`SYSTEM.region_pal` and lines per frame through the `region_detect` ROM).
// Every adjustment is listed in docs/clean-room-register.md.
#pragma once
#include "md/cart.hpp"
#include "md/machine.hpp"

#include <cstdint>
#include <span>

namespace tessera::md {

/// What the user asks of the console: the header decides, or a fixed model.
enum class RegionChoice : std::uint8_t {
  automatic,
  japan,
  usa,
  europe,
  japan_pal, ///< 50 Hz Japanese console (header bit 2); not reachable on gpgx
};

namespace region_detail {
inline constexpr std::uint8_t kJapanNtsc = 1, kJapanPal = 2, kAmericas = 4,
                              kEurope = 8;
/// The four bytes at $1F0 are read one by one and their bits are or-ed:
/// J/U/E (either case) set one bit and any other hex digit contributes its
/// own four bits. `E` is always the letter, never the digit 14.
constexpr std::uint8_t header_mask(std::span<const std::uint8_t> rom) noexcept {
  std::uint8_t mask = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    const char c = static_cast<char>(rom[0x1F0 + i]);
    if (c == 'J' || c == 'j')
      mask |= kJapanNtsc;
    else if (c == 'U' || c == 'u')
      mask |= kAmericas;
    else if (c == 'E' || c == 'e')
      mask |= kEurope;
    else if (c >= '0' && c <= '9')
      mask |= static_cast<std::uint8_t>(c - '0');
    else if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
      mask |= static_cast<std::uint8_t>((c | 0x20) - 'a' + 10);
  }
  return mask;
}
} // namespace region_detail

/// Region declared by the header of a plain (header-less, de-interleaved) ROM
/// image. Several regions: Americas, then Japan NTSC, then Europe, then Japan
/// PAL. No recognised region (or an image too short for the field): Americas.
constexpr MachineConfig region_from_header(
    std::span<const std::uint8_t> rom) noexcept {
  using namespace region_detail;
  const std::uint8_t mask = rom.size() >= 0x1F4 ? header_mask(rom) : 0;
  if (mask & kAmericas || mask == 0)
    return MachineConfig::for_region(Region::usa);
  if (mask & kJapanNtsc)
    return MachineConfig::for_region(Region::japan);
  if (mask & kEurope)
    return MachineConfig::for_region(Region::europe);
  auto config = MachineConfig::for_region(Region::japan);
  config.standard = Standard::pal;
  return config;
}

/// Machine of a given choice for a plain ROM image.
constexpr MachineConfig
region_config(RegionChoice choice, std::span<const std::uint8_t> rom) noexcept {
  switch (choice) {
  case RegionChoice::japan:
    return MachineConfig::for_region(Region::japan);
  case RegionChoice::usa:
    return MachineConfig::for_region(Region::usa);
  case RegionChoice::europe:
    return MachineConfig::for_region(Region::europe);
  case RegionChoice::japan_pal: {
    auto config = MachineConfig::for_region(Region::japan);
    config.standard = Standard::pal;
    return config;
  }
  case RegionChoice::automatic:
    break;
  }
  return region_from_header(rom);
}

} // namespace tessera::md
