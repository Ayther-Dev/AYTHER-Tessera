// `delta_probe` (spec 008, TP-205.4): a Mega Drive test ROM that writes VRAM,
// CRAM, VSRAM and VDP registers from a per-frame table, so the frame delta
// (dirty patterns) of each frame is known. No commercial content: a SEGA
// header and hand-assembled 68000 words.
//
// The level 6 handler counts frames in $FF0000. The main loop waits for the
// counter to move, then runs the table entries of that frame:
//   1, command high, command low, count, value, step: writes the command long
//      to the control port, then `count` words to the data port, the value
//      growing by `step` each time (count 0 only writes the command: DMA);
//   2, word: writes one word to the control port (a register);
//   0: end of the frame; 0xFFFF: idle for good.
#pragma once
#include "md_testrom.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace tessera::testrom {

struct DeltaTable {
  std::vector<std::uint16_t> words;
  DeltaTable &vram(std::uint32_t address, std::uint16_t count,
                   std::uint16_t value, std::uint16_t step = 0) {
    return command(0x4000 | (address & 0x3FFF), (address >> 14) & 3, count,
                   value, step);
  }
  DeltaTable &cram(std::uint32_t address, std::uint16_t count,
                   std::uint16_t value, std::uint16_t step = 0) {
    return command(0xC000 | (address & 0x7F), 0, count, value, step);
  }
  DeltaTable &vsram(std::uint32_t address, std::uint16_t count,
                    std::uint16_t value, std::uint16_t step = 0) {
    return command(0x4000 | (address & 0x7F), 0x10, count, value, step);
  }
  DeltaTable &command(std::uint16_t high, std::uint16_t low,
                      std::uint16_t count, std::uint16_t value,
                      std::uint16_t step) {
    words.insert(words.end(), {1, high, low, count, value, step});
    return *this;
  }
  DeltaTable &reg(std::uint16_t word) {
    words.insert(words.end(), {2, word});
    return *this;
  }
  /// VRAM fill of `length` bytes at `address` with the high byte of `value`.
  DeltaTable &fill(std::uint32_t address, std::uint16_t length,
                   std::uint16_t value) {
    reg(0x8F01).reg(0x9300 | (length & 0xFF)).reg(0x9400 | (length >> 8));
    reg(0x9780);
    command(0x4000 | (address & 0x3FFF), 0x80 | ((address >> 14) & 3), 1, value,
            0);
    return *this;
  }
  /// VRAM copy of `length` bytes from `source` to `address`.
  DeltaTable &copy(std::uint32_t source, std::uint32_t address,
                   std::uint16_t length) {
    reg(0x8F01).reg(0x9300 | (length & 0xFF)).reg(0x9400 | (length >> 8));
    reg(0x9500 | (source & 0xFF)).reg(0x9600 | ((source >> 8) & 0xFF));
    reg(0x97C0);
    command(address & 0x3FFF, 0xC0 | ((address >> 14) & 3), 0, 0, 0);
    return *this;
  }
  /// 68000 DMA of `words` words from ROM `source` to VRAM `address`
  /// (`target` 0), CRAM (1) or VSRAM (2).
  DeltaTable &dma(std::uint32_t source, std::uint32_t address,
                  std::uint16_t words, unsigned target = 0) {
    reg(0x9300 | (words & 0xFF)).reg(0x9400 | (words >> 8));
    reg(0x9500 | ((source >> 1) & 0xFF)).reg(0x9600 | ((source >> 9) & 0xFF));
    reg(0x9700 | ((source >> 17) & 0x7F));
    const std::uint16_t high[] = {
        static_cast<std::uint16_t>(0x4000 | (address & 0x3FFF)),
        static_cast<std::uint16_t>(0xC000 | (address & 0x7F)),
        static_cast<std::uint16_t>(0x4000 | (address & 0x7F))};
    const std::uint16_t low[] = {
        static_cast<std::uint16_t>(0x80 | ((address >> 14) & 3)), 0x80, 0x90};
    command(high[target], low[target], 0, 0, 0);
    return *this;
  }
  DeltaTable &end_frame() {
    words.push_back(0);
    return *this;
  }
};

/// The frames of each variant; out of range gives variant 0.
inline DeltaTable delta_variant(unsigned variant) {
  DeltaTable t;
  switch (variant) {
  default:
  case 0: // single writes: which pattern, which row, same value again
    t.vram(0x0000, 1, 0x1111).end_frame(); // frame 0: pattern 0 row 0
    t.vram(0x0022, 1, 0x2222).end_frame(); // pattern 1 row 0
    t.vram(0x003E, 1, 0x3333).vram(0xFFFE, 1, 0x4444).end_frame();
    t.end_frame();                                  // nothing
    t.vram(0x0100, 16, 0x5555, 0x0101).end_frame(); // pattern 8 whole
    t.vram(0x0000, 1, 0x1111).end_frame();          // same value again
    t.cram(0x0000, 4, 0x0EEE).vsram(0x0000, 2, 0x0010).end_frame();
    t.vram(0x0041, 1, 0x6666).end_frame(); // odd address
    t.vram(0x0044, 1, 0x7777).vram(0x0048, 1, 0x7777).end_frame();
    t.reg(0x8230).reg(0x8407).reg(0x8578).end_frame(); // registers only
    t.vram(0x8000, 1, 0x1234).vram(0xC000, 1, 0x1234).end_frame();
    t.vram(0x0000, 0x400, 0x0000, 1).end_frame(); // 64 patterns
    break;
  case 1: // DMA
    t.fill(0x0400, 0x40, 0xAB00).reg(0x8F02).end_frame();
    t.copy(0x0400, 0x0800, 0x40).reg(0x8F02).end_frame();
    t.dma(0x1000, 0x0C00, 0x20).end_frame();
    t.dma(0x1000, 0x0000, 0x10, 1).end_frame();
    t.dma(0x1000, 0x0000, 0x10, 2).end_frame();
    t.fill(0x2001, 0x21, 0xCD00).reg(0x8F02).end_frame(); // odd start
    t.end_frame();
    break;
  case 2: // one pattern a frame, for history and since
    for (unsigned f = 0; f < 24; ++f)
      t.vram(0x20u * (f + 1) + 2u * (f % 16), 1,
             static_cast<std::uint16_t>(0x0100 + f))
          .end_frame();
    break;
  case 4: // the same values again, through each kind of write
    t.dma(0x1000, 0x0C00, 0x20).end_frame();
    t.dma(0x1000, 0x0C00, 0x20).end_frame();
    t.fill(0x0400, 0x40, 0xAB00).reg(0x8F02).end_frame();
    t.fill(0x0400, 0x40, 0xAB00).reg(0x8F02).end_frame();
    t.copy(0x0400, 0x0800, 0x40).reg(0x8F02).end_frame();
    t.copy(0x0400, 0x0800, 0x40).reg(0x8F02).end_frame();
    t.vram(0x3000, 16, 0x0000).end_frame(); // zeros over zeros
    t.vram(0x0C00, 1, 0x0102).vram(0x0C22, 1, 0x0000).end_frame();
    t.end_frame();
    break;
  case 3: // all of VRAM in one frame
    t.end_frame();
    t.vram(0x0000, 0x8000, 0x0101, 0x0101).end_frame();
    t.end_frame();
    break;
  }
  t.words.push_back(0xFFFF);
  return t;
}

inline std::vector<std::uint8_t> delta_probe_rom(unsigned variant) {
  std::vector<std::uint8_t> rom(0x8000, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  put32(rom, 0x78, 0x300); // level 6 autovector
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER DELTA    ", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  // clang-format off
  const std::uint16_t code[] = {
      0x41F9, 0x00C0, 0x0000, // $200 lea    $C00000,a0
      0x43F9, 0x00C0, 0x0004, // $206 lea    $C00004,a1
      0x32BC, 0x8004,         // $20C move.w #$8004,(a1)
      0x32BC, 0x8174,         // $210 move.w #$8174,(a1) display, VINT, DMA
      0x32BC, 0x8C81,         // $214 move.w #$8C81,(a1) H40
      0x32BC, 0x8F02,         // $218 move.w #$8F02,(a1) increment 2
      0x4279, 0x00FF, 0x0000, // $21C clr.w  $FF0000
      0x45F9, 0x0000, 0x0400, // $222 lea    $400,a2 (table)
      0x46FC, 0x2000,         // $228 move   #$2000,sr
      0x3039, 0x00FF, 0x0000, // $22C frame: move.w $FF0000,d0
      0xB079, 0x00FF, 0x0000, // $232 wait:  cmp.w  $FF0000,d0
      0x67F8,                 // $238        beq.s  wait
      0x321A,                 // $23A next:  move.w (a2)+,d1
      0x67EE,                 // $23C        beq.s  frame
      0x6B1C,                 // $23E        bmi.s  halt
      0x0C41, 0x0002,         // $240        cmpi.w #2,d1
      0x6712,                 // $244        beq.s  reg
      0x229A,                 // $246        move.l (a2)+,(a1)
      0x341A,                 // $248        move.w (a2)+,d2
      0x361A,                 // $24A        move.w (a2)+,d3
      0x381A,                 // $24C        move.w (a2)+,d4
      0x5342,                 // $24E write: subq.w #1,d2
      0x6BE8,                 // $250        bmi.s  next
      0x3083,                 // $252        move.w d3,(a0)
      0xD644,                 // $254        add.w  d4,d3
      0x60F6,                 // $256        bra.s  write
      0x329A,                 // $258 reg:   move.w (a2)+,(a1)
      0x60DE,                 // $25A        bra.s  next
      0x60FE,                 // $25C halt:  bra.s  halt
  };
  const std::uint16_t handler[] = {
      0x5279, 0x00FF, 0x0000, // $300 addq.w #1,$FF0000
      0x4E73,                 //      rte
  };
  // clang-format on
  std::size_t at = 0x200;
  for (const auto word : code) {
    put16(rom, at, word);
    at += 2;
  }
  at = 0x300;
  for (const auto word : handler) {
    put16(rom, at, word);
    at += 2;
  }
  at = 0x400;
  for (const auto word : delta_variant(variant).words) {
    put16(rom, at, word);
    at += 2;
  }
  for (unsigned i = 0; i < 0x40; ++i) // DMA source at $1000
    put16(rom, 0x1000 + 2 * i, static_cast<std::uint16_t>(0x0102 * (i + 1)));
  return rom;
}

} // namespace tessera::testrom
