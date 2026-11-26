// Cheat codes of the native Mega Drive core (spec 008, TP-111, RF-4.3).
//
// Both device families decode to the same thing: one 16 bit word written at a
// 24 bit even address of the 68000 space. An address below $400000 patches the
// cartridge (what the cartridge answers to a read); an address in the work RAM
// area ($E00000-$FFFFFF) rewrites that RAM. Anything else is refused.
//
// Formats (public documentation of the devices, no emulator source):
//  - Game Genie: eight characters of ABCDEFGHJKLMNPRSTVWXYZ0123456789 (5 bits
//    each, 'I' reads as 1 and 'O' as 0), optionally written AAAA-BBBB. The 40
//    bits are scattered over the address and the data; kGameGenie* below.
//  - Pro Action Replay: six hexadecimal digits of address and four of data,
//    written AAAAAA DDDD, AAAAAA:DDDD or run together.
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace tessera::md {

struct Cheat {
  std::uint32_t address; ///< even, 24 bit
  std::uint16_t data;
};

namespace cheat_detail {
/// Value of a Game Genie character, -1 if it is not one.
constexpr int genie_value(char c) noexcept {
  constexpr std::string_view alphabet = "ABCDEFGHJKLMNPRSTVWXYZ0123456789";
  if (c >= 'a' && c <= 'z')
    c = static_cast<char>(c - 'a' + 'A');
  if (c == 'I')
    c = '1';
  if (c == 'O')
    c = '0';
  const auto at = alphabet.find(c);
  return at == std::string_view::npos ? -1 : static_cast<int>(at);
}
constexpr int hex_value(char c) noexcept {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
/// Bits lo..hi (inclusive) of `v`.
constexpr std::uint32_t bits(unsigned v, unsigned lo, unsigned hi) noexcept {
  return (v >> lo) & ((1U << (hi - lo + 1)) - 1U);
}
constexpr bool addressable(const Cheat &c) noexcept {
  const bool rom = c.address < 0x400000;
  const bool ram = c.address >= 0xE00000 && c.address <= 0xFFFFFF;
  return (c.address & 1U) == 0 && (rom || ram);
}
} // namespace cheat_detail

/// True if the cheat rewrites work RAM, false if it patches the cartridge.
constexpr bool cheat_targets_ram(const Cheat &c) noexcept {
  return c.address >= 0xE00000;
}

/// Decodes a Game Genie code; nullopt if it is malformed or its address is not
/// one the console answers (odd, or outside ROM and work RAM).
constexpr std::optional<Cheat> decode_game_genie(std::string_view code) {
  unsigned v[8] = {};
  unsigned count = 0;
  for (const char c : code) {
    if (c == '-' && count == 4)
      continue;
    if (count == 8)
      return std::nullopt;
    const int value = cheat_detail::genie_value(c);
    if (value < 0)
      return std::nullopt;
    v[count++] = static_cast<unsigned>(value);
  }
  if (count != 8)
    return std::nullopt;
  using cheat_detail::bits;
  Cheat out{};
  out.address = bits(v[7], 0, 4) | bits(v[6], 0, 2) << 5 |
                bits(v[3], 4, 4) << 8 | bits(v[2], 0, 4) << 9 |
                bits(v[1], 0, 1) << 14 | bits(v[4], 1, 4) << 16 |
                bits(v[3], 0, 3) << 20;
  out.data = static_cast<std::uint16_t>(
      bits(v[1], 2, 4) | bits(v[0], 0, 4) << 3 | bits(v[5], 1, 4) << 8 |
      bits(v[4], 0, 0) << 12 | bits(v[6], 3, 4) << 13 | bits(v[5], 0, 0) << 15);
  if (!cheat_detail::addressable(out))
    return std::nullopt;
  return out;
}

/// Decodes a Pro Action Replay code; same refusals as the Game Genie.
constexpr std::optional<Cheat> decode_par(std::string_view code) {
  unsigned count = 0;
  std::uint32_t address = 0, data = 0;
  for (const char c : code) {
    if ((c == ' ' || c == ':') && count == 6)
      continue;
    const int value = cheat_detail::hex_value(c);
    if (value < 0 || count == 10)
      return std::nullopt;
    if (count < 6)
      address = address << 4 | static_cast<unsigned>(value);
    else
      data = data << 4 | static_cast<unsigned>(value);
    ++count;
  }
  if (count != 10)
    return std::nullopt;
  Cheat out{address, static_cast<std::uint16_t>(data)};
  if (!cheat_detail::addressable(out))
    return std::nullopt;
  return out;
}

/// Either family: a code of eight Genie characters (with or without the
/// hyphen) is a Game Genie code; ten hexadecimal digits are a PAR code.
constexpr std::optional<Cheat> decode_cheat(std::string_view code) {
  if (auto genie = decode_game_genie(code))
    return genie;
  return decode_par(code);
}

} // namespace tessera::md
