#include "md/cart.hpp"

#include "domain/result.hpp"

namespace tessera::md {
namespace {

constexpr std::size_t kSmdBlock = 0x4000;

std::uint32_t big32(std::span<const std::uint8_t> b, std::size_t at) {
  return (std::uint32_t(b[at]) << 24) | (std::uint32_t(b[at + 1]) << 16) |
         (std::uint32_t(b[at + 2]) << 8) | b[at + 3];
}

bool has_sega(std::span<const std::uint8_t> b) {
  if (b.size() < 0x104)
    return false;
  const auto at = [&](std::size_t o) {
    return b[o] == 'S' && b[o + 1] == 'E' && b[o + 2] == 'G' && b[o + 3] == 'A';
  };
  return at(0x100) || at(0x101); // "SEGA..." or " SEGA" (TMSS-style)
}

bool is_smd_header(std::span<const std::uint8_t> h) {
  return h.size() >= kCopierHeader && h[1] == 0x03 && h[8] == 0xAA &&
         h[9] == 0xBB;
}

std::vector<std::uint8_t> deinterleave(std::span<const std::uint8_t> data) {
  std::vector<std::uint8_t> out(data.size());
  for (std::size_t block = 0; block + kSmdBlock <= data.size();
       block += kSmdBlock) {
    for (std::size_t i = 0; i < kSmdBlock / 2; ++i) {
      out[block + 2 * i + 1] = data[block + i];
      out[block + 2 * i] = data[block + kSmdBlock / 2 + i];
    }
  }
  return out;
}

std::uint8_t parse_regions(std::span<const std::uint8_t> rom) {
  std::uint8_t mask = 0;
  bool letters = false;
  for (std::size_t i = 0; i < 3; ++i) {
    const char c = static_cast<char>(rom[0x1F0 + i]);
    if (c == 'J') {
      mask |= kRegionJapan;
      letters = true;
    }
    if (c == 'U') {
      mask |= kRegionUsa;
      letters = true;
    }
    if (c == 'E') {
      mask |= kRegionEurope;
      letters = true;
    }
  }
  if (letters)
    return mask;
  // New style: one hexadecimal digit, bit 0 Japan, bit 2 Americas, bit 3
  // Europe.
  const char c = static_cast<char>(rom[0x1F0]);
  int value = -1;
  if (c >= '0' && c <= '9')
    value = c - '0';
  else if (c >= 'A' && c <= 'F')
    value = c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    value = c - 'a' + 10;
  if (value < 0)
    return 0;
  if (value & 1)
    mask |= kRegionJapan;
  if (value & 4)
    mask |= kRegionUsa;
  if (value & 8)
    mask |= kRegionEurope;
  return mask;
}

SramInfo parse_sram(std::span<const std::uint8_t> rom) {
  SramInfo sram;
  if (rom[0x1B0] != 'R' || rom[0x1B1] != 'A')
    return sram;
  const auto start = big32(rom, 0x1B4);
  const auto end = big32(rom, 0x1B8);
  if (end < start)
    return sram;
  sram.present = true;
  sram.start = start;
  sram.end = end;
  switch (rom[0x1B2]) {
  case 0xF8:
    sram.bus = SramBus::odd;
    break;
  case 0xE8:
    sram.bus = SramBus::even;
    break;
  default:
    sram.bus = SramBus::both;
    break;
  }
  const std::uint32_t range = end - start + 1;
  sram.size_bytes = sram.bus == SramBus::both ? range : (range + 1) / 2;
  return sram;
}

} // namespace

SramInfo read_sram_header(std::span<const std::uint8_t> rom) {
  if (rom.size() < 0x1BC)
    return {};
  return parse_sram(rom);
}

Cart load_cart(std::span<const std::uint8_t> content) {
  require(!content.empty() && content.size() <= kMaxRomSize + kCopierHeader,
          TESSERA_INVALID_CONTENT);
  Cart cart;
  if (has_sega(content)) {
    cart.format = CartFormat::plain;
    cart.rom.assign(content.begin(), content.end());
  } else {
    require(content.size() > kCopierHeader, TESSERA_INVALID_CONTENT);
    const auto payload = content.subspan(kCopierHeader);
    if (is_smd_header(content) && payload.size() % kSmdBlock == 0) {
      cart.format = CartFormat::smd;
      cart.rom = deinterleave(payload);
    } else {
      cart.format = CartFormat::copier_header;
      cart.rom.assign(payload.begin(), payload.end());
    }
  }
  require(cart.rom.size() <= kMaxRomSize && cart.rom.size() >= 0x200 &&
              (cart.rom.size() & 1U) == 0 && has_sega(cart.rom),
          TESSERA_INVALID_CONTENT);
  cart.regions = parse_regions(cart.rom);
  cart.sram = parse_sram(cart.rom);
  return cart;
}

} // namespace tessera::md
