// Test ROMs for the limits of plan 2.10 (spec 008, TP-208.3, RNF-5): hand
// assembled 68000, no commercial content.
#pragma once

#include "support/md_testrom.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace tessera::testrom {

/// `audio_flood`: the 68000 writes the PSG port ($C00011) without pause, an
/// unrolled block of `writes` move.b d0,(a0) (8 cycles each) closed by a
/// branch back, so a frame holds about 16 000 writes, more than the 8 192
/// records of AUDIO_WRITES. d0 counts up by one at the end of every block, so
/// the records of consecutive blocks differ.
inline std::vector<std::uint8_t> audio_flood_rom(unsigned writes = 4096) {
  std::vector<std::uint16_t> c = {
      0x41F9,
      0x00C0,
      0x0011, // lea $C00011,a0 (PSG)
      0x7000, // moveq #0,d0
  };
  const std::size_t loop = c.size();
  for (unsigned i = 0; i < writes; ++i)
    c.push_back(0x1080); // move.b d0,(a0)
  c.push_back(0x5200);   // addq.b #1,d0
  const auto back = static_cast<std::int32_t>(loop * 2) -
                    static_cast<std::int32_t>(c.size() * 2 + 2);
  c.insert(c.end(), {0x6000, static_cast<std::uint16_t>(back)}); // bra.w loop
  const std::size_t size = (0x200 + c.size() * 2 + 0xFFF) & ~std::size_t{0xFFF};
  std::vector<std::uint8_t> rom(size, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER AUDIOFLOOD", 17);
  put32(rom, 0x1A0, 0);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(size - 1));
  put32(rom, 0x1A8, 0xFF0000);
  put32(rom, 0x1AC, 0xFFFFFF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  for (std::size_t i = 0; i < c.size(); ++i)
    put16(rom, 0x200 + i * 2, c[i]);
  return rom;
}

/// A minimal valid cartridge of exactly `bytes` bytes (even, >= 0x400): it
/// idles in a loop; the header declares the whole size.
inline std::vector<std::uint8_t> sized_rom(std::size_t bytes) {
  std::vector<std::uint8_t> rom(bytes, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER SIZED    ", 16);
  put32(rom, 0x1A0, 0);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(bytes - 1));
  put32(rom, 0x1A8, 0xFF0000);
  put32(rom, 0x1AC, 0xFFFFFF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  put16(rom, 0x200, 0x60FE); // bra.s *
  return rom;
}

} // namespace tessera::testrom
