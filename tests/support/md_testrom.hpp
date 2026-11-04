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
  // Work RAM accesses of every width (byte, word, long, a 64 KiB mirror and
  // a subroutine call that pushes the return address), then the SRAM sweep.
  const std::uint16_t code[] = {
      0x23FC, 0x0123, 0x4567, 0x00FF, 0x0000, // $200 move.l #$01234567,$FF0000
      0x33FC, 0x89AB, 0x00FF, 0x0004,         // $20A move.w #$89AB,$FF0004
      0x13FC, 0x00CD, 0x00FF, 0x0006,         // $212 move.b #$CD,$FF0006
      0x33FC, 0x1357, 0x00E2, 0x0008,         // $21A move.w #$1357,$E20008
      0x6100, 0x003C,                         // $222 bsr.w  $260
      0x13FC, 0x0001, 0x00A1, 0x30F1,         // $226 move.b #1,$A130F1
      0x41F9, 0x0020, 0x0001,                 // $22E lea    $200001,a0
      0x303C, 0x1FFF,                         // $234 move.w #$1FFF,d0
      0x1080,                                 // $238 loop: move.b d0,(a0)
      0x5488,                                 //      addq.l #2,a0
      0x51C8, 0xFFFA,                         //      dbra   d0,loop
      0x60FE,                                 // $240 hang: bra.s hang
  };
  std::size_t at = 0x200;
  for (const auto word : code) {
    put16(rom, at, word);
    at += 2;
  }
  const std::uint16_t subroutine[] = {
      0x23FC, 0xDEAD, 0xBEEF, 0x00FF, 0x0010, // $260 move.l #$DEADBEEF,$FF0010
      0x4E75,                                 //      rts
  };
  at = 0x260;
  for (const auto word : subroutine) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `bus_sweep` (SSF2 part): an 8 MiB image of 16 banks of 512 KiB. Every bank
// carries its own number at its first, middle and last byte, so a read through
// the mapper tells which bank answered.
inline std::vector<std::uint8_t> ssf2_sweep_rom() {
  std::vector<std::uint8_t> rom(8U * 1024U * 1024U, 0);
  for (std::size_t bank = 0; bank < 16; ++bank) {
    const std::size_t base = bank * 0x80000;
    rom[base + 0x40000] = static_cast<std::uint8_t>(0xB0 + bank);
    rom[base + 0x7FFFF] = static_cast<std::uint8_t>(0xC0 + bank);
    rom[base + 0x2000] = static_cast<std::uint8_t>(0xD0 + bank);
  }
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA SSF        ", 16);
  put32(rom, 0x1A4, 0x7FFFFF);
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `vint_counter`: sets VDP register 1 to enable the vertical interrupt, opens
// the 68000 interrupt mask and idles; the level 6 handler adds one to the long
// at $FF0000 (work RAM). After N frames the counter holds N.
inline std::vector<std::uint8_t> vint_counter_rom() {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  put32(rom, 0x78, 0x300); // level 6 autovector
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER VINT CNT ", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  const std::uint16_t main_code[] = {
      0x33FC, 0x8164,
      0x00C0, 0x0004, // move.w #$8164,$C00004 (reg1: display, VINT)
      0x46FC, 0x2000, // move   #$2000,sr
      0x60FE,         // bra.s  .
  };
  std::size_t at = 0x200;
  for (const auto word : main_code) {
    put16(rom, at, word);
    at += 2;
  }
  const std::uint16_t handler[] = {
      0x52B9,
      0x00FF,
      0x0000, // addq.l #1,$FF0000
      0x4E73, // rte
  };
  at = 0x300;
  for (const auto word : handler) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `dma_fifo`: runs the three DMA classes (68000 bus to VRAM, CRAM and VSRAM, a
// VRAM fill and a VRAM copy), records the status word right after the fill
// and reads VRAM, CRAM and VSRAM back into work RAM from $FF0000: 4 status
// words, then 512 + 64 + 40 words. The bench compares that RAM with gpgx.
inline std::vector<std::uint8_t> dma_fifo_rom() {
  std::vector<std::uint8_t> rom(0x480, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER DMA FIFO ", 16);
  put32(rom, 0x1A4, 0x47F);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  for (unsigned i = 0; i < 32; ++i) // DMA source at $400
    put16(rom, 0x400 + 2 * i,
          static_cast<std::uint16_t>(0x0123 + i * 0x0111 + (i << 12)));
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0000, // lea $C00000,a0
      0x43F9, 0x00C0, 0x0004, // lea $C00004,a1
      0x45F9, 0x00FF, 0x0000, // lea $FF0000,a2
      0x32BC, 0x8114,         // mode 5 + DMA enabled (before registers >= 11)
      0x32BC, 0x8F02,         // autoincrement 2
  };
  const auto regs = [&](std::uint16_t length, std::uint16_t reg23,
                        std::uint16_t reg22 = 0x9602) {
    for (const std::uint16_t w :
         {static_cast<std::uint16_t>(0x9300 | (length & 0xFF)),
          static_cast<std::uint16_t>(0x9400 | (length >> 8)),
          std::uint16_t{0x9500}, reg22, reg23}) {
      c.push_back(0x32BC);
      c.push_back(w);
    }
  };
  const auto command = [&](std::uint16_t first, std::uint16_t second) {
    c.insert(c.end(), {0x22BC, first, second});
  };
  const auto poll = [&] { // wait until the busy bit (status bit 1) clears
    c.insert(c.end(), {0x3011, 0x0240, 0x0002, 0x66F8});
  };
  regs(16, 0x9700);
  command(0x4000, 0x0080); // 68000 -> VRAM at 0
  regs(8, 0x9700);
  command(0xC000, 0x0080); // 68000 -> CRAM at 0
  regs(8, 0x9700);
  command(0x4000, 0x0090); // 68000 -> VSRAM at 0
  regs(0x20, 0x9780);
  command(0x4100, 0x0080);             // VRAM fill at $100
  c.insert(c.end(), {0x30BC, 0xABCD}); // fill data
  for (int i = 0; i < 4; ++i) // 4 status reads, bits 1 (busy) and 3 (blank)
    c.insert(c.end(), {0x3011, 0x0240, 0x000A, 0x34C0});
  poll();
  regs(0x10, 0x97C0, 0x9600);
  command(0x0200, 0x00C0); // VRAM copy to $200
  poll();
  // The bits the VDP does not drive come from the bus (prefetch), so every
  // read is masked to the bits the memory really holds.
  const auto readback = [&](std::uint16_t second, std::uint16_t words,
                            std::uint16_t mask) {
    command(0x0000, second);
    c.insert(c.end(), {0x303C, static_cast<std::uint16_t>(words - 1), 0x3010,
                       0x0240, mask, 0x34C0, 0x51C8, 0xFFF6});
  };
  readback(0x0000, 512, 0xFFFF); // VRAM
  readback(0x0020, 64, 0x0EEE);  // CRAM
  readback(0x0010, 40, 0x07FF);  // VSRAM
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `hv_probe` (interrupt part, TP-106.2): enables the horizontal interrupt with
// a line counter of `hint_reg` (register 10) and the vertical one; the level
// 4 handler adds one to the long at $FF0000 and the level 6 handler to the
// long at $FF0004. After N frames both hold how many interrupts were taken.
inline std::vector<std::uint8_t> hint_counter_rom(std::uint8_t hint_reg) {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  put32(rom, 0x70, 0x300); // level 4 autovector
  put32(rom, 0x78, 0x310); // level 6 autovector
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER HINT CNT ", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  const std::uint16_t main_code[] = {
      0x33FC, 0x8164,
      0x00C0, 0x0004, // reg1: mode 5, display, VINT
      0x33FC, 0x8014,
      0x00C0, 0x0004, // reg0: HINT enabled
      0x33FC, static_cast<std::uint16_t>(0x8A00 | hint_reg),
      0x00C0, 0x0004,
      0x47F9, 0x00FF,
      0x0100,         // lea $FF0100,a3 (per frame HINT counts)
      0x46FC, 0x2000, // move #$2000,sr
      0x60FE,         // bra.s .
  };
  std::size_t at = 0x200;
  for (const auto word : main_code) {
    put16(rom, at, word);
    at += 2;
  }
  const std::uint16_t hint[] = {0x52B9, 0x00FF, 0x0000, 0x4E73};
  const std::uint16_t vint[] = {
      0x52B9, 0x00FF, 0x0004, // addq.l #1,$FF0004
      0x2039, 0x00FF, 0x0000, // move.l $FF0000,d0
      0x42B9, 0x00FF, 0x0000, // clr.l  $FF0000
      0x16C0,                 // move.b d0,(a3)+
      0x4E73,                 // rte
  };
  at = 0x300;
  for (const auto word : hint) {
    put16(rom, at, word);
    at += 2;
  }
  at = 0x310;
  for (const auto word : vint) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `hv_probe` (V counter part, TP-106.1): the level 6 handler samples the V
// counter ($C00008, high byte) in a loop from the first line after the display
// and stores every distinct value, in order, from $FF0010 until the counter
// wraps to 0. The list does not depend on the CPU timing, only on how the V
// counter steps, so it shows the jumps of each mode. `reg1_extra` can add the
// V30 bit (0x08) and `reg12` selects H32/H40.
inline std::vector<std::uint8_t> hv_probe_rom(std::uint8_t reg1_extra,
                                              std::uint8_t reg12) {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  put32(rom, 0x78, 0x310); // level 6 autovector
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER HV PROBE ", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  const std::uint16_t main_code[] = {
      0x33FC, static_cast<std::uint16_t>(0x8164 | reg1_extra),
      0x00C0, 0x0004,
      0x33FC, static_cast<std::uint16_t>(0x8C00 | reg12),
      0x00C0, 0x0004,
      0x46FC, 0x2000, // move #$2000,sr
      0x60FE,         // bra.s .
  };
  std::size_t at = 0x200;
  for (const auto word : main_code) {
    put16(rom, at, word);
    at += 2;
  }
  const std::uint16_t handler[] = {
      0x41F9, 0x00C0, 0x0008, // lea $C00008,a0
      0x45F9, 0x00FF, 0x0010, // lea $FF0010,a2
      0x123C, 0x00FF,         // move.b #$FF,d1
      0x3010,                 // loop: move.w (a0),d0
      0xE048,                 //       lsr.w  #8,d0
      0xB001,                 //       cmp.b  d1,d0
      0x67F8,                 //       beq    loop
      0x14C0,                 //       move.b d0,(a2)+
      0x1200,                 //       move.b d0,d1
      0x4A00,                 //       tst.b  d0
      0x66F0,                 //       bne    loop
      0x4E73,                 //       rte
  };
  at = 0x310;
  for (const auto word : handler) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `hv_probe` (H counter part, TP-106.1): samples the H counter ($C00009) in a
// tight loop and sets byte `$FF0100 + value` for every value seen, so after a
// few frames the table lists which H values exist in the mode (the jump of the
// counter), whatever the CPU timing. `reg12` selects H32 (0) or H40 ($81).
inline std::vector<std::uint8_t> hv_h_probe_rom(std::uint8_t reg12) {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER H PROBE  ", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  const std::uint16_t code[] = {
      0x33FC, 0x8104,
      0x00C0, 0x0004, // mode 5
      0x33FC, static_cast<std::uint16_t>(0x8C00 | reg12),
      0x00C0, 0x0004,
      0x41F9, 0x00C0,
      0x0008, // lea $C00008,a0
      0x45F9, 0x00FF,
      0x0100,         // lea $FF0100,a2
      0x7201,         // moveq #1,d1
      0x3010,         // loop: move.w (a0),d0
      0x0240, 0x00FF, //       andi.w #$FF,d0
      0x1581, 0x0000, //       move.b d1,(a2,d0.w)
      0x4E71,         //       nop (44 cycle loop: samples every phase)
      0x60F2,         //       bra loop
  };
  std::size_t at = 0x200;
  for (const auto word : code) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `mode4_pbc` (TP-104.6): the VDP stays in Mode 4 (register 1 bit 2 clear), as
// a Power Base Converter game leaves it. It writes VRAM with the byte and word
// accesses of the 68000 through the control and data ports and reads it back
// with byte and word reads into work RAM from $FF0000.
inline std::vector<std::uint8_t> mode4_pbc_rom() {
  std::vector<std::uint8_t> rom(0x600, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER MODE4PBC ", 16);
  put32(rom, 0x1A4, 0x5FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0001, // lea $C00001,a0 (data port, byte access)
      0x43F9, 0x00C0, 0x0004, // lea $C00004,a1 (control port)
      0x45F9, 0x00FF, 0x0000, // lea $FF0000,a2
      0x47F9, 0x00C0, 0x0000, // lea $C00000,a3 (data port, word access)
  };
  // Mode 4: one word is a whole command. A long write would make its second
  // word a new command (address 0), as measured on gpgx.
  const auto command = [&](std::uint16_t first, std::uint16_t) {
    c.insert(c.end(), {0x32BC, first}); // move.w #imm,(a1)
  };
  const auto write_byte = [&](std::uint16_t v) {
    c.insert(c.end(), {0x10BC, v}); // move.b #v,(a0)
  };
  const auto write_word = [&](std::uint16_t v) {
    c.insert(c.end(), {0x36BC, v}); // move.w #v,(a3)
  };
  const auto read_bytes = [&](int n) {
    for (int i = 0; i < n; ++i)
      c.push_back(0x14D0); // move.b (a0),(a2)+
  };
  const auto read_words = [&](int n) {
    for (int i = 0; i < n; ++i)
      c.push_back(0x34D3); // move.w (a3),(a2)+
  };
  command(0x4000, 0); // three byte writes, four byte reads
  write_byte(0x11);
  write_byte(0x22);
  write_byte(0x33);
  command(0x0000, 0);
  read_bytes(4);
  command(0x4010, 0); // two word writes, four byte reads
  write_word(0x4455);
  write_word(0x6677);
  command(0x0010, 0);
  read_bytes(4);
  command(0x4020, 0); // one byte write, two word reads
  write_byte(0x88);
  command(0x0020, 0);
  read_words(2);
  command(0x4030, 0); // read command at an odd address
  write_byte(0xAA);
  write_byte(0xBB);
  command(0x0031, 0);
  read_bytes(3);
  command(0x4041, 0); // write command at an odd address
  write_byte(0xCC);
  command(0x0040, 0);
  read_bytes(4);
  command(0x4005, 0x0001); // address bits 14-15 do not exist in Mode 4
  write_byte(0x77);
  command(0x0005, 0);
  read_bytes(2);
  // CRAM in Mode 4, read back in Mode 5 (word index = address & $1F)
  c.insert(c.end(), {0x45F9, 0x00FF, 0x0020}); // lea $FF0020,a2: aligned
  command(0xC001, 0);
  write_word(0x0E4A);
  write_word(0x0123);
  command(0xC020, 0);
  write_word(0x0E4A);
  c.insert(c.end(), {0x32BC, 0x8144});         // register 1: Mode 5, display on
  c.insert(c.end(), {0x22BC, 0x0000, 0x0020}); // CRAM read at 0 (Mode 5)
  read_words(3);
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom
