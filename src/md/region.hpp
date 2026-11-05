// Console region of the native Mega Drive (spec 008, TP-108.4, PQ-13): the
// region is detected from the ROM header, with manual selection on top.
// The rule is the one gpgx documents as observable behaviour: the country
// field ($1F0, four characters) names the regions a cartridge supports and the
// console takes the first of USA, Japan NTSC, Europe, Japan PAL; a few
// cartridges with a wrong header are recognised by product code and checksum.
// The region fixes the version register (overseas, PAL bits) and the video
// standard (262 or 313 lines).
#pragma once
#include "md/machine.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace tessera::md {

/// Console variants. Japan PAL has no cartridge-side detection (it is only
/// reachable by hand), as in the oracle.
enum class RegionChoice : std::uint8_t {
  automatic,
  usa,
  europe,
  japan_ntsc,
  japan_pal
};

struct RegionSetup {
  Region region = Region::usa;
  Standard standard = Standard::ntsc;
  constexpr bool operator==(const RegionSetup &) const noexcept = default;
};

namespace region_detail {
inline std::uint32_t big16(std::span<const std::uint8_t> rom,
                           std::size_t at) noexcept {
  return (std::uint32_t{rom[at]} << 8) | rom[at + 1];
}
inline char upper(char c) noexcept {
  return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}
/// Bit mask of the countries of the header: 1 Japan NTSC, 2 Japan PAL,
/// 4 USA, 8 Europe.
inline int country_mask(std::span<const std::uint8_t> rom) noexcept {
  const auto *country = reinterpret_cast<const char *>(rom.data() + 0x1F0);
  const std::string_view head(country, 3);
  if (head == "eur" || head == "EUR" || head == "Eur")
    return 8;
  if (head == "jap" || head == "JAP")
    return 1;
  if (head == "usa" || head == "USA")
    return 4;
  int mask = 0;
  for (int i = 0; i < 4; ++i) {
    const char c = upper(country[i]);
    if (c == 'U')
      mask |= 4;
    else if (c == 'J')
      mask |= 1;
    else if (c == 'E')
      mask |= 8;
    else if (c == 'K')
      mask |= 1;
    else if (c < 16)
      mask |= c;
    else if (c >= '0' && c <= '9')
      mask |= c - '0';
    else if (c >= 'A' && c <= 'F')
      mask |= c - 'A' + 10;
  }
  return mask;
}
inline bool product_has(std::span<const std::uint8_t> rom,
                        std::string_view needle) noexcept {
  std::string_view product(reinterpret_cast<const char *>(rom.data() + 0x182),
                           12);
  if (const auto end = product.find('\0'); end != std::string_view::npos)
    product = product.substr(0, end);
  return product.find(needle) != std::string_view::npos;
}
inline std::uint16_t real_checksum(std::span<const std::uint8_t> rom) noexcept {
  std::uint16_t sum = 0;
  for (std::size_t i = 0x200; i + 1 < rom.size(); i += 2)
    sum = static_cast<std::uint16_t>(sum + big16(rom, i));
  return sum;
}
} // namespace region_detail

/// Region of the console for a cartridge, from its header. `rom` is the plain
/// de-interleaved image (at least 0x200 bytes; a shorter one gives USA).
inline RegionSetup detect_region(std::span<const std::uint8_t> rom) noexcept {
  using namespace region_detail;
  if (rom.size() < 0x200)
    return {Region::usa, Standard::ntsc};
  const auto checksum = big16(rom, 0x18E);
  const auto real = real_checksum(rom);
  // Cartridges whose header names the wrong region.
  if ((product_has(rom, "T-45033") &&
       checksum == 0x0F81) ||            // Alisia Dragon (EU)
      product_has(rom, "T-69046-50") ||  // Back to the Future III (EU)
      product_has(rom, "T-120106-00") || // Brian Lara Cricket (EU)
      product_has(rom, "T-97126 -50") || // Williams Arcade's Greatest Hits (EU)
      product_has(rom, "T-113026-50") || // Wiz'n'Liz (EU)
      product_has(rom, "T-70096 -00") || // Muhammad Ali Heavyweight Boxing (EU)
      (checksum == 0x0000 && real == 0x1F7F)) // Sensible Soccer Plus (Radica)
    return {Region::europe, Standard::pal};
  if (real == 0x532E && product_has(rom, "1011-00")) // On Dal Jang Goon (KR)
    return {Region::japan, Standard::ntsc};
  const int mask = country_mask(rom);
  if (mask & 4)
    return {Region::usa, Standard::ntsc};
  if (mask & 1)
    return {Region::japan, Standard::ntsc};
  if (mask & 8)
    return {Region::europe, Standard::pal};
  if (mask & 2)
    return {Region::japan, Standard::pal};
  return {Region::usa, Standard::ntsc};
}

/// The region of a manual choice, or the detected one for `automatic`.
inline RegionSetup resolve_region(std::span<const std::uint8_t> rom,
                                  RegionChoice choice) noexcept {
  switch (choice) {
  case RegionChoice::usa:
    return {Region::usa, Standard::ntsc};
  case RegionChoice::europe:
    return {Region::europe, Standard::pal};
  case RegionChoice::japan_ntsc:
    return {Region::japan, Standard::ntsc};
  case RegionChoice::japan_pal:
    return {Region::japan, Standard::pal};
  case RegionChoice::automatic:
    break;
  }
  return detect_region(rom);
}

/// Machine configuration with the region and the video standard resolved for
/// `rom` (pads and the rest keep their defaults).
inline MachineConfig
config_for_rom(std::span<const std::uint8_t> rom,
               RegionChoice choice = RegionChoice::automatic) noexcept {
  const auto setup = resolve_region(rom, choice);
  MachineConfig config;
  config.region = setup.region;
  config.standard = setup.standard;
  return config;
}

} // namespace tessera::md
