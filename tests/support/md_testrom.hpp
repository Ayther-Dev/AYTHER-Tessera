// Synthetic Mega Drive test ROMs (spec 008, TP-101.10). They contain no
// commercial content: a SEGA header and a few hand-assembled 68000 words.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace tessera::testrom {

inline void put32(std::vector<std::uint8_t> &b, std::size_t at,
                  std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
}
inline void put16(std::vector<std::uint8_t> &b, std::size_t at,
                  std::uint16_t v) {
  b[at] = static_cast<std::uint8_t>(v >> 8);
  b[at + 1] = static_cast<std::uint8_t>(v);
}

// `bus_sweep` (SRAM part): enables the cartridge SRAM through $A130F1 and
// writes 8192 bytes into the odd addresses $200001-$203FFF, the value being
// the low byte of the countdown 0x1FFF..0, then idles. The header declares
// "RA" $F8 (8 bit, odd addresses) over that range.
inline std::vector<std::uint8_t> bus_sweep_rom() {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00); // initial SP
  put32(rom, 4, 0x200);    // initial PC
  const char console[] = "SEGA MEGA DRIVE ";
  std::memcpy(&rom[0x100], console, 16);
  std::memcpy(&rom[0x120], "AYTHER BUS SWEEP", 16);
  put32(rom, 0x1A0, 0);
  put32(rom, 0x1A4, 0x3FF);
  put32(rom, 0x1A8, 0xFF0000);
  put32(rom, 0x1AC, 0xFFFFFF);
  rom[0x1B0] = 'R';
  rom[0x1B1] = 'A';
  rom[0x1B2] = 0xF8;
  rom[0x1B3] = 0x20;
  put32(rom, 0x1B4, 0x200001);
  put32(rom, 0x1B8, 0x203FFF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  const std::uint16_t code[] = {
      0x13FC, 0x0001, 0x00A1, 0x30F1, // move.b #1,$A130F1
      0x41F9, 0x0020, 0x0001,         // lea    $200001,a0
      0x303C, 0x1FFF,                 // move.w #$1FFF,d0
      0x1080,                         // loop: move.b d0,(a0)
      0x5488,                         //       addq.l #2,a0
      0x51C8, 0xFFFA,                 //       dbra   d0,loop
      0x60FE,                         // hang: bra.s  hang
  };
  std::size_t at = 0x200 - 0x200; // code lives at $200; ROM is 0x400 bytes
  at = 0x200;
  for (const auto word : code) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom
