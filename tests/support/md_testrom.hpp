// Synthetic Mega Drive test ROMs (spec 008, TP-101.10). They contain no
// commercial content: a SEGA header and a few hand-assembled 68000 words.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
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
  // clang-format off
  const std::uint16_t handler[] = {
      0x52B9,
      0x00FF,
      0x0000, // addq.l #1,$FF0000
      0x4E73, // rte
  };
  at = 0x300;
  // clang-format on
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

// `fifo_timing` (TP-104.3): writes 64 words to the data port, each one
// followed by a read of the HV counter ($C00008) stored at $FF0000 + 2 * n.
// A write that finds the FIFO full stalls the 68000, so the sequence of HV
// words is the observable of the FIFO timing. `target` 0 = VRAM, 1 = CRAM,
// 2 = VSRAM; `display` leaves the display on (reg 1 bit 6); `h40` selects the
// 40 cell mode; `in_vblank` starts the burst at the first line of the blanking
// instead of the first line of the active display.
inline std::vector<std::uint8_t> fifo_timing_rom(unsigned target, bool display,
                                                 bool h40, bool in_vblank,
                                                 bool reads_only = false,
                                                 unsigned count = 64,
                                                 bool long_write = false) {
  std::vector<std::uint8_t> rom(0x400 + 4 * count, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER FIFO TIME", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0,
      0x0000, // lea $C00000,a0
      0x43F9, 0x00C0,
      0x0004, // lea $C00004,a1
      0x47F9, 0x00C0,
      0x0008, // lea $C00008,a3
      0x45F9, 0x00FF,
      0x0000,         // lea $FF0000,a2
      0x32BC, 0x8144, // mode 5, display on
      0x32BC, 0x8F02, // autoincrement 2
      0x32BC, static_cast<std::uint16_t>(h40 ? 0x8C81 : 0x8C00),
  };
  const auto wait_status = [&](bool until_set) { // status bit 3 (vblank)
    c.insert(c.end(),
             {0x3011, 0x0240, 0x0008,
              static_cast<std::uint16_t>(until_set ? 0x67F8 : 0x66F8)});
  };
  wait_status(true);  // inside the blanking
  wait_status(false); // first line of the active display
  if (in_vblank) {
    wait_status(true); // first line of the next blanking
  }
  c.insert(c.end(),
           {0x32BC, static_cast<std::uint16_t>(display ? 0x8144 : 0x8104)});
  const std::uint16_t second = target == 0   ? 0x0000
                               : target == 1 ? 0x0000
                                             : 0x0010;
  const std::uint16_t first = target == 0   ? 0x4000
                              : target == 1 ? 0xC000
                                            : 0x4000;
  c.insert(c.end(), {0x22BC, first, second});
  c.insert(c.end(), {0x303C, 0x0EEE}); // d0 = data
  for (unsigned i = 0; i < count; ++i)
    c.insert(c.end(), {reads_only ? std::uint16_t{0x4E71}
                                  : std::uint16_t(long_write ? 0x2080 : 0x3080),
                       0x34D3}); // move.w d0,(a0) or nop; move.w (a3),(a2)+
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `cpu_drift` (TP-104.3): after syncing to the end of the vertical blanking
// the 68000 runs a dbf loop of 250 * k iterations (10 cycles each, no VDP
// access) and reads the HV counter into $FF0000 + 2 * (k - 1), k = 1..32.
// The difference between emulators grows with the time if one of them takes
// cycles away from the CPU (DRAM refresh).
inline std::vector<std::uint8_t> cpu_drift_rom(bool h40) {
  std::vector<std::uint8_t> rom(0x600, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER CPU DRIFT", 16);
  put32(rom, 0x1A4, 0x5FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x43F9, 0x00C0,
      0x0004, // lea $C00004,a1
      0x47F9, 0x00C0,
      0x0008, // lea $C00008,a3
      0x45F9, 0x00FF,
      0x0000,         // lea $FF0000,a2
      0x32BC, 0x8144, // mode 5, display on (the blanking flag moves)
      0x32BC, static_cast<std::uint16_t>(h40 ? 0x8C81 : 0x8C00),
  };
  for (unsigned k = 1; k <= 32; ++k) {
    // wait for blanking, then for its end
    c.insert(c.end(), {0x3011, 0x0240, 0x0008, 0x67F8});
    c.insert(c.end(), {0x3011, 0x0240, 0x0008, 0x66F8});
    c.insert(c.end(),
             {0x323C, static_cast<std::uint16_t>(250 * k - 1), 0x51C9, 0xFFFE});
    c.insert(c.end(), {0x34D3}); // move.w (a3),(a2)+
  }
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `dma_timing` (TP-104.4): after the sync of fifo_timing, runs 68000 to VDP
// DMAs of 2, 4, 8 ... 256 words from the ROM to VRAM (`target` 0) or CRAM
// (1), each followed by a read of the HV counter stored at $FF0000 + 2 * n.
// The 68000 is held until the DMA ends, so the HV words are its durations.
inline std::vector<std::uint8_t> dma_timing_rom(unsigned target, bool display,
                                                bool h40, bool in_vblank) {
  std::vector<std::uint8_t> rom(0x800, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER DMA TIME ", 16);
  put32(rom, 0x1A4, 0x7FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0000,
      0x43F9, 0x00C0, 0x0004,
      0x47F9, 0x00C0, 0x0008,
      0x45F9, 0x00FF, 0x0000,
      0x32BC, 0x8154, 0x32BC,
      0x8F02, 0x32BC, static_cast<std::uint16_t>(h40 ? 0x8C81 : 0x8C00),
  };
  const auto wait_status = [&](bool until_set) {
    c.insert(c.end(),
             {0x3011, 0x0240, 0x0008,
              static_cast<std::uint16_t>(until_set ? 0x67F8 : 0x66F8)});
  };
  for (unsigned n = 0; n < 11; ++n) {
    c.insert(c.end(), {0x32BC, std::uint16_t{0x8154}}); // display on to sync
    wait_status(true);
    wait_status(false);
    if (in_vblank)
      wait_status(true);
    c.insert(c.end(),
             {0x32BC, static_cast<std::uint16_t>(display ? 0x8154 : 0x8114)});
    const std::uint16_t length = static_cast<std::uint16_t>(2U << n);
    for (const std::uint16_t w :
         {static_cast<std::uint16_t>(0x9300 | (length & 0xFF)),
          static_cast<std::uint16_t>(0x9400 | (length >> 8)),
          std::uint16_t{0x9500}, std::uint16_t{0x9680},
          std::uint16_t{0x9700}}) {
      c.push_back(0x32BC);
      c.push_back(w);
    }
    c.insert(c.end(),
             {0x22BC, static_cast<std::uint16_t>(target == 0 ? 0x4000 : 0xC000),
              0x0080});
    c.insert(c.end(), {0x34D3});
  }
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `dma_fill_timing`: VRAM fill (or copy) of 2, 4 ... 2048 bytes; the 68000
// is not held, so it polls the busy bit (status bit 1) and then stores the HV
// counter at $FF0000 + 2 * n: the words are the durations of the fills.
inline std::vector<std::uint8_t> dma_fill_timing_rom(bool copy, bool display,
                                                     bool h40) {
  std::vector<std::uint8_t> rom(0x800, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER FILL TIM ", 16);
  put32(rom, 0x1A4, 0x7FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0000,
      0x43F9, 0x00C0, 0x0004,
      0x47F9, 0x00C0, 0x0008,
      0x45F9, 0x00FF, 0x0000,
      0x32BC, 0x8154, 0x32BC,
      0x8F02, 0x32BC, static_cast<std::uint16_t>(h40 ? 0x8C81 : 0x8C00),
  };
  const auto wait_status = [&](bool until_set) {
    c.insert(c.end(),
             {0x3011, 0x0240, 0x0008,
              static_cast<std::uint16_t>(until_set ? 0x67F8 : 0x66F8)});
  };
  for (unsigned n = 0; n < 11; ++n) {
    c.insert(c.end(), {0x32BC, std::uint16_t{0x8154}});
    wait_status(true);
    wait_status(false);
    c.insert(c.end(),
             {0x32BC, static_cast<std::uint16_t>(display ? 0x8154 : 0x8114)});
    const std::uint16_t length = static_cast<std::uint16_t>(2U << n);
    for (const std::uint16_t w :
         {static_cast<std::uint16_t>(0x9300 | (length & 0xFF)),
          static_cast<std::uint16_t>(0x9400 | (length >> 8)),
          std::uint16_t{0x9500}, std::uint16_t{0x9600},
          static_cast<std::uint16_t>(copy ? 0x97C0 : 0x9780)}) {
      c.push_back(0x32BC);
      c.push_back(w);
    }
    if (copy) {
      c.insert(c.end(), {0x22BC, 0x0000, 0x00C0});
    } else {
      c.insert(c.end(), {0x22BC, 0x4000, 0x0080});
      c.insert(c.end(), {0x30BC, 0x1234});
    }
    c.insert(c.end(), {0x3011, 0x0240, 0x0002, 0x66F8}); // while busy
    c.insert(c.end(), {0x34D3});
  }
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `z80_busreq` (TP-103.3): requests the Z80 bus, counts the polls of $A11100
// until it is granted and stores the count at $FF0000 + 2 * n, 16 times with
// a growing delay before each request; also reads the status after the
// release.
inline std::vector<std::uint8_t> z80_busreq_rom() {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER Z80 BUSRQ", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x45F9, 0x00FF, 0x0000, // lea $FF0000,a2
      0x49F9, 0x00A1, 0x1100, // lea $A11100,a4
      0x4BF9, 0x00A1, 0x1200, // lea $A11200,a5
      0x3ABC, 0x0100,         // reset released
  };
  for (unsigned n = 0; n < 16; ++n) {
    c.insert(c.end(),
             {0x323C, static_cast<std::uint16_t>(n * 3), 0x51C9, 0xFFFE});
    c.insert(c.end(), {0x38BC, 0x0100, 0x7200});
    c.insert(c.end(), {0x5241, 0x0814, 0x0000, 0x66F8});
    c.insert(c.end(), {0x34C1});
    c.insert(c.end(), {0x38BC, 0x0000});
  }
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `ym_busy` (TP-103, SoR2 corpus): with the Z80 bus granted the 68000 writes a
// YM2612 address and a data byte and logs 48 status reads after each (from
// $FF0000). The busy flag (bit 7) after a write is what the games poll.
inline std::vector<std::uint8_t> ym_busy_rom(unsigned nops = 0) {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER YM BUSY  ", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x45F9, 0x00FF, 0x0000, // lea $FF0000,a2
      0x49F9, 0x00A1, 0x1100, // lea $A11100,a4
      0x4BF9, 0x00A1, 0x1200, // lea $A11200,a5
      0x41F9, 0x00A0, 0x4000, // lea $A04000,a0
      0x3ABC, 0x0100,         // reset released
      0x38BC, 0x0100,         // bus request
      0x0814, 0x0000, 0x66FA, // wait for the grant
      0x10BC, 0x002B,         // move.b #$2B,(a0): address
  };
  for (unsigned i = 0; i < 48; ++i)
    c.push_back(0x14D0);                       // move.b (a0),(a2)+
  c.insert(c.end(), {0x117C, 0x0000, 0x0001}); // move.b #0,1(a0): data
  for (unsigned i = 0; i < nops; ++i)
    c.push_back(0x4E71);
  for (unsigned i = 0; i < 48; ++i)
    c.push_back(0x14D0);
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `cpu_timing` (TP-101.8): 40 repetitions of each instruction of a battery,
// each group followed by a read of the HV counter stored at
// $FF0000 + 2 * group. The differences between emulators are per instruction
// cycle counts. `group_count` is the number of groups of the battery.
struct CpuGroup {
  std::vector<std::uint16_t> setup; // before every repetition
  std::vector<std::uint16_t> code;
};
inline std::vector<CpuGroup> cpu_timing_groups() {
  const std::vector<std::uint16_t> d0 = {0x203C, 0x1234, 0x5678};
  const std::vector<std::uint16_t> d0s = {0x203C, 0x0001, 0x2345};
  return {
      {{}, {0x4E71}},                   // nop
      {d0, {0xC0C1}},                   // mulu d1,d0
      {d0, {0xC1C1}},                   // muls d1,d0
      {d0s, {0x80C1}},                  // divu d1,d0
      {d0s, {0x81C1}},                  // divs d1,d0
      {d0, {0xE5A8}},                   // lsl.l d2,d0
      {d0, {0xE468}},                   // lsr.w d2,d0
      {d0, {0xE680}},                   // asr.l #3,d0
      {d0, {0xE7B8}},                   // rol.l d2,d0
      {d0, {0xE390}},                   // roxl.l #1,d0
      {{}, {0x48D1, 0x00F0}},           // movem.l d4-d7,(a1)
      {{}, {0x4CD1, 0x00F0}},           // movem.l (a1),d4-d7
      {{}, {0x22D8}},                   // move.l (a0)+,(a1)+
      {{}, {0xD195}},                   // add.l d0,(a5)
      {d0, {0xD181}},                   // addx.l d1,d0
      {d0, {0xC101}},                   // abcd d1,d0
      {{}, {0x4AD5}},                   // tas (a5)
      {{}, {0x4295}},                   // clr.l (a5)
      {{}, {0x4855}},                   // pea (a5)
      {{}, {0x6102, 0x4E71, 0x4E71}},   // bsr.s
      {d0, {0x0500}},                   // btst d2,d0
      {d0, {0x05C0}},                   // bset d2,d0
      {d0, {0x48C0, 0x4840, 0xC141}},   // ext.l, swap, exg
      {{}, {0xB348}},                   // cmpm.w (a0)+,(a1)+
      {d0, {0x54C0}},                   // scc d0
      {{}, {0x4A81, 0x6702, 0x4E71}},   // tst.l, beq.s (not taken)
      {{}, {0x4A81, 0x6602, 0x4E71}},   // tst.l, bne.s (taken)
      {{}, {0x2300}},                   // move.l d0,-(a1)
      {{}, {0x4E56, 0xFFFC, 0x4E5E}},   // link, unlk
      {d0, {0xC0FC, 0xFFFF}},           // mulu #$FFFF,d0
      {d0, {0x80FC, 0x0003}},           // divu #3,d0
      {{}, {0xB090}},                   // cmp.l (a0),d0
      {{}, {0x5395}},                   // subq.l #1,(a5)
      {d0, {0x4480, 0x4680}},           // neg.l, not.l
      {{}, {0x3010, 0x3290}},           // move.w (a0),d0; move.w (a0),(a1)
      {{}, {0x1030, 0x0000}},           // move.b 0(a0,d0.w),d0
      {{}, {0x41E8, 0x0010}},           // lea 16(a0),a0 (pointer walks)
      {{}, {0x4E75 - 0x4E75 + 0x4E71}}, // nop (reference)
  };
}
inline unsigned cpu_timing_group_count() { return 38; }

inline std::vector<CpuGroup> cpu_z80area_groups() {
  return {
      {{}, {0x4E71}},                 // nop
      {{}, {0x1016}},                 // move.b (a6),d0   Z80 RAM
      {{}, {0x3016}},                 // move.w (a6),d0
      {{}, {0x1C80}},                 // move.b d0,(a6)
      {{}, {0x3C80}},                 // move.w d0,(a6)
      {{}, {0x1019}},                 // move.b (a1)+,d0  work RAM
      {{}, {0x1039, 0x00A1, 0x1100}}, // move.b $A11100,d0
      {{}, {0x1039, 0x00A0, 0x4000}}, // move.b $A04000,d0  YM2612
      {{}, {0x1039, 0x00C0, 0x0011}}, // move.b $C00011,d0  PSG
      {{}, {0x1039, 0x00A1, 0x0001}}, // move.b $A10001,d0  I/O
      {{}, {0x1039, 0x0000, 0x0100}}, // move.b $100,d0     ROM
  };
}

inline std::vector<CpuGroup> cpu_muldiv_groups() {
  std::vector<CpuGroup> g;
  const std::uint16_t values[8] = {0x0001, 0x0003, 0x00FF, 0x0100,
                                   0x5555, 0x8000, 0xAAAA, 0xFFFF};
  for (const std::uint16_t op : {0xC0C1, 0xC1C1, 0x80C1, 0x81C1})
    for (const std::uint16_t v : values)
      g.push_back({{0x223C, 0x0000, v, 0x203C, 0x0000, 0x7FFF}, {op}});
  return g;
}

inline std::vector<std::uint8_t>
cpu_timing_rom(const std::vector<CpuGroup> &groups = cpu_timing_groups()) {
  std::vector<std::uint8_t> rom(0x10000, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER CPU TIME ", 16);
  put32(rom, 0x1A4, 0xFFFF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x43F9, 0x00FF, 0x5000,         // lea $FF5000,a1
      0x41F9, 0x00FF, 0x4000,         // lea $FF4000,a0
      0x4BF9, 0x00FF, 0x6000,         // lea $FF6000,a5
      0x47F9, 0x00C0, 0x0008,         // lea $C00008,a3
      0x45F9, 0x00FF, 0x0000,         // lea $FF0000,a2
      0x49F9, 0x00C0, 0x0004,         // lea $C00004,a4
      0x38BC, 0x8144,                 // mode 5, display on
      0x38BC, 0x8C81,                 // H40
      0x243C, 0x0000, 0x0009,         // move.l #9,d2
      0x223C, 0x0000, 0x00FF,         // move.l #$FF,d1
      0x4DF9, 0x00A0, 0x0000,         // lea $A00000,a6
      0x33FC, 0x0100, 0x00A1, 0x1100, // bus request
      0x33FC, 0x0100, 0x00A1, 0x1200, // reset released
  };
  // the HV read sits in memory (a2) so the work RAM must not be touched by
  // the groups: they use a5/a1/a0 areas only
  for (const auto &g : groups) {
    c.insert(c.end(), {0x3614, 0x0243, 0x0008, 0x67F8}); // wait: blanking
    c.insert(c.end(), {0x3614, 0x0243, 0x0008, 0x66F8}); // wait: active
    for (unsigned i = 0; i < 40; ++i) {
      c.insert(c.end(), g.setup.begin(), g.setup.end());
      c.insert(c.end(), g.code.begin(), g.code.end());
    }
    c.push_back(0x34D3); // move.w (a3),(a2)+
  }
  c.push_back(0x60FE);
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `cpu_drift_long` (TP-104.3): like cpu_drift but with loops of 1.3 frames
// repeated K = 1, 2, 4 ... 32 times, so the stolen cycles of a DRAM refresh
// add up to many lines. HV words at $FF0000 + 2 * n.
inline std::vector<std::uint8_t> cpu_drift_long_rom() {
  std::vector<std::uint8_t> rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER DRIFT LNG", 16);
  put32(rom, 0x1A4, 0x3FF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::vector<std::uint16_t> c = {
      0x45F9, 0x00FF, 0x0000, 0x47F9, 0x00C0, 0x0008, 0x49F9,
      0x00C0, 0x0004, 0x38BC, 0x8144, 0x38BC, 0x8C81,
  };
  for (unsigned k : {1U, 2U, 4U, 8U, 16U, 32U}) {
    c.insert(c.end(), {0x3614, 0x0243, 0x0008, 0x67F8});
    c.insert(c.end(), {0x3614, 0x0243, 0x0008, 0x66F8});
    c.insert(c.end(), {0x363C, static_cast<std::uint16_t>(k - 1), 0x343C,
                       0xFFFE, 0x51CA, 0xFFFE, 0x51CB, 0xFFF6});
    c.push_back(0x34D3);
  }
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

// `hv_probe` for PAL (TP-106.1): the same sampling, but it does not stop at the
// first wrap to 0: it keeps the first 100 distinct values, so the list shows
// the jump of the PAL counter ($102 -> $CA in V28, $10A -> $D2 in V30) that
// comes after the wrap. The region byte of the header is 'E' so that gpgx
// picks 50 Hz.
inline std::vector<std::uint8_t> hv_pal_probe_rom(std::uint8_t reg1_extra) {
  auto rom = hv_probe_rom(reg1_extra, 0x00);
  rom[0x1F0] = 'E';
  const std::uint16_t handler[] = {
      0x41F9, 0x00C0, 0x0008, // lea $C00008,a0
      0x45F9, 0x00FF, 0x0010, // lea $FF0010,a2
      0x123C, 0x00FF,         // move.b #$FF,d1
      0x7463,                 // moveq #99,d2
      0x3010,                 // loop: move.w (a0),d0
      0xE048,                 //       lsr.w  #8,d0
      0xB001,                 //       cmp.b  d1,d0
      0x67F8,                 //       beq    loop
      0x14C0,                 //       move.b d0,(a2)+
      0x1200,                 //       move.b d0,d1
      0x51CA, 0xFFF2,         //       dbra   d2,loop
      0x4E73,                 //       rte
  };
  std::size_t at = 0x310;
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

namespace tessera::testrom {

// A VDP scene ROM (spec 008, TP-105): a header, then unrolled 68000 code that
// loads registers, VRAM, CRAM, VSRAM and finally turns the display on, then
// idles. The bench compares the frame with gpgx, so what matters is that both
// machines see exactly the same writes.
struct Scene {
  std::vector<std::pair<std::uint8_t, std::uint8_t>> regs; // before the data
  std::vector<std::pair<std::uint16_t, std::vector<std::uint16_t>>> vram;
  std::vector<std::uint16_t> cram;  // from index 0
  std::vector<std::uint16_t> vsram; // from index 0
  std::uint8_t reg1 = 0x44;         // final value of register 1
  bool poll_status = false;         // accumulate status bits 5-6 in work RAM
};

inline std::vector<std::uint8_t> scene_rom(const Scene &scene,
                                           const char (&title)[17]) {
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0000, // lea $C00000,a0 (data port)
      0x43F9, 0x00C0, 0x0004, // lea $C00004,a1 (control port)
  };
  const auto reg = [&](std::uint8_t r, std::uint8_t v) {
    c.insert(c.end(),
             {0x32BC, static_cast<std::uint16_t>(0x8000 | (r << 8) | v)});
  };
  const auto command = [&](std::uint32_t cmd) {
    c.insert(c.end(), {0x22BC, static_cast<std::uint16_t>(cmd >> 16),
                       static_cast<std::uint16_t>(cmd)});
  };
  const auto data = [&](std::uint16_t w) { c.insert(c.end(), {0x30BC, w}); };
  reg(0, 0x04); // full palette
  reg(1, 0x04); // display off, Mode 5
  reg(15, 2);
  for (const auto &[r, v] : scene.regs)
    reg(r, v);
  for (const auto &[addr, words] : scene.vram) {
    command((0x40000000U | ((addr & 0x3FFFU) << 16)) | ((addr >> 14) & 3U));
    for (const auto w : words)
      data(w);
  }
  if (!scene.cram.empty()) {
    command(0xC0000000U);
    for (const auto w : scene.cram)
      data(w);
  }
  if (!scene.vsram.empty()) {
    command(0x40000010U);
    for (const auto w : scene.vsram)
      data(w);
  }
  reg(1, scene.reg1);
  if (scene.poll_status) {
    // d1 |= status & $60, stored at $FF0000, forever (overflow and collision
    // flags are cleared by every read, so the loop accumulates them).
    c.insert(c.end(), {0x7200,                 // moveq #0,d1
                       0x3011,                 // move.w (a1),d0
                       0x0240, 0x0070,         // andi.w #$70,d0
                       0x8240,                 // or.w d0,d1
                       0x33C1, 0x00FF, 0x0000, // move.w d1,$FF0000
                       0x60F0});               // bra loop
  } else {
    c.push_back(0x60FE); // bra.s .
  }
  std::vector<std::uint8_t> rom(0x200 + c.size() * 2 + 2, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], title, 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

/// Deterministic pseudo random numbers for scene contents.
struct Lcg {
  std::uint32_t state;
  std::uint32_t next() {
    state = state * 1664525U + 1013904223U;
    return state >> 8;
  }
};

/// `palette_sweep` block `block` (0..8): 60 CRAM colours (colour indices
/// 1..15 of the four palette lines), shown as solid cells of plane B: rows 0-1
/// with the priority bit (normal) and rows 2-3 without (shadow, as register
/// 12 enables shadow/highlight). Nine blocks cover the 512 colours.
inline std::vector<std::uint8_t> palette_sweep_rom(unsigned block) {
  Scene scene;
  scene.regs = {{2, 0x08}, {4, 0x06}, {12, 0x08}, {16, 0x00}, {7, 0x00}};
  std::vector<std::uint16_t> tiles;
  for (unsigned t = 1; t <= 15; ++t)
    for (unsigned i = 0; i < 16; ++i)
      tiles.push_back(static_cast<std::uint16_t>(t * 0x1111U));
  scene.vram.emplace_back(0x0020, tiles);
  scene.cram.assign(64, 0);
  for (unsigned line = 0; line < 4; ++line)
    for (unsigned colour = 1; colour < 16; ++colour) {
      const unsigned v = (block * 60U + line * 15U + colour - 1U) % 512U;
      scene.cram[line * 16 + colour] = static_cast<std::uint16_t>(
          ((v >> 6) & 7U) << 9 | ((v >> 3) & 7U) << 5 | (v & 7U) << 1);
    }
  std::vector<std::uint16_t> table(32 * 4, 0);
  for (unsigned shadow = 0; shadow < 2; ++shadow)
    for (unsigned n = 0; n < 60; ++n) {
      const unsigned line = n / 15U, colour = n % 15U + 1U;
      const unsigned cell = (shadow * 2U + n / 30U) * 32U + n % 30U;
      table[cell] = static_cast<std::uint16_t>((shadow == 0 ? 0x8000U : 0U) |
                                               (line << 13) | colour);
    }
  scene.vram.emplace_back(0xC000, table);
  return scene_rom(scene, "AYTHER PALSWEEP ");
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `modes_sweep` variant: random but deterministic planes. Both planes use the
// same 64 patterned tiles, random attributes and a random CRAM, VSRAM and
// horizontal scroll table, so that any scroll mode, size, flip, priority or
// window rule that differs from gpgx moves pixels.
struct ModesVariant {
  std::uint8_t reg11 = 0x00;      // scroll modes
  std::uint8_t reg16 = 0x00;      // plane size
  std::uint8_t reg12 = 0x00;      // H32/H40, shadow/highlight, interlace
  std::uint8_t reg1_extra = 0x00; // V30
  std::uint8_t reg3 = 0x34;       // window name table
  std::uint8_t reg17 = 0x00;      // window horizontal
  std::uint8_t reg18 = 0x00;      // window vertical
  std::uint8_t reg0_extra = 0x00; // left column blank
  std::uint8_t reg7 = 0x05;
  bool limited_palette = false; // register 0 bit 2 clear
  int hscroll_a = -1;           // first horizontal scroll entry of plane A
};

inline std::vector<std::uint8_t> modes_sweep_rom(const ModesVariant &v) {
  Scene scene;
  scene.regs = {{2, 0x10},   // plane A at $4000
                {4, 0x06},   // plane B at $C000
                {3, v.reg3}, // window
                {13, 0x38},  // horizontal scroll table at $E000
                {5, 0x7F},   // sprite table far away, empty
                {11, v.reg11}, {16, v.reg16}, {12, v.reg12},
                {17, v.reg17}, {18, v.reg18}, {7, v.reg7}};
  if (v.limited_palette)
    scene.regs.insert(scene.regs.begin(), {0, 0x00});
  else if (v.reg0_extra != 0)
    scene.regs.insert(scene.regs.begin(),
                      {0, static_cast<std::uint8_t>(0x04 | v.reg0_extra)});
  scene.reg1 = static_cast<std::uint8_t>(0x44 | v.reg1_extra);
  Lcg rng{0x5EED0001U};
  // 64 tiles with a pattern that is not symmetric: flips are visible.
  std::vector<std::uint16_t> tiles;
  for (unsigned t = 0; t < 64; ++t)
    for (unsigned row = 0; row < 8; ++row) {
      std::uint32_t word = 0;
      for (unsigned col = 0; col < 8; ++col) {
        const unsigned colour =
            (t == 0) ? 0U : (((t * 7U + row * 3U + col * 5U) % 15U) + 1U);
        const unsigned c = (rng.next() % 5U == 0U) ? 0U : colour; // holes
        word = (word << 4) | c;
      }
      tiles.push_back(static_cast<std::uint16_t>(word >> 16));
      tiles.push_back(static_cast<std::uint16_t>(word));
    }
  scene.vram.emplace_back(0x0000, tiles);
  const unsigned cells_x = ((v.reg16 & 3U) == 0)   ? 32U
                           : ((v.reg16 & 3U) == 1) ? 64U
                                                   : 128U;
  const unsigned cells_y = (((v.reg16 >> 4) & 3U) == 0)   ? 32U
                           : (((v.reg16 >> 4) & 3U) == 1) ? 64U
                                                          : 128U;
  const unsigned entries = std::min(cells_x * cells_y, 4096U);
  const auto table = [&](bool priorities) {
    std::vector<std::uint16_t> t(entries);
    for (auto &e : t) {
      const std::uint32_t r = rng.next();
      e = static_cast<std::uint16_t>(
          (priorities && (r & 0x100U) ? 0x8000U : 0U) |
          (((r >> 9) & 3U) << 13) | ((r & 0x200U) ? 0x1000U : 0U) |
          ((r & 0x400U) ? 0x0800U : 0U) | ((r >> 12) % 64U));
    }
    return t;
  };
  scene.vram.emplace_back(0x4000, table(true));
  scene.vram.emplace_back(0xC000, table(true));
  std::vector<std::uint16_t> window(64 * 32);
  for (auto &e : window) {
    const std::uint32_t r = rng.next();
    e = static_cast<std::uint16_t>(((r & 0x100U) ? 0x8000U : 0U) |
                                   (((r >> 9) & 3U) << 13) | ((r >> 12) % 64U));
  }
  scene.vram.emplace_back(0xD000, window);
  std::vector<std::uint16_t> hscroll(240 * 2);
  for (auto &e : hscroll)
    e = static_cast<std::uint16_t>(rng.next() % 1024U);
  if (v.hscroll_a >= 0)
    hscroll[0] = static_cast<std::uint16_t>(v.hscroll_a);
  scene.vram.emplace_back(0xE000, hscroll);
  scene.cram.resize(64);
  for (auto &c : scene.cram)
    c = static_cast<std::uint16_t>(rng.next() & 0x0EEEU);
  scene.vsram.resize(40);
  for (auto &e : scene.vsram)
    e = static_cast<std::uint16_t>(rng.next() % 1024U);
  return scene_rom(scene, "AYTHER MODESWEEP");
}

/// Variant `id` of `modes_sweep`; the list is mirrored in
/// tools/diffbench/CMakeLists.txt (one test per variant).
inline ModesVariant modes_variant(unsigned id) {
  ModesVariant v;
  switch (id) {
  case 0: // plain 32x32, full scroll
    break;
  case 1: // 64x32, cell horizontal scroll
    v.reg11 = 0x02;
    v.reg16 = 0x01;
    break;
  case 2: // 32x64, line horizontal scroll
    v.reg11 = 0x03;
    v.reg16 = 0x10;
    break;
  case 3: // 64x64, 2 cell vertical scroll
    v.reg11 = 0x04;
    v.reg16 = 0x11;
    break;
  case 4: // H40, 128x32, line scroll and 2 cell vertical scroll
    v.reg11 = 0x07;
    v.reg12 = 0x81;
    v.reg16 = 0x03;
    break;
  case 5: // invalid horizontal scroll mode
    v.reg11 = 0x01;
    break;
  case 6: // V30, H40
    v.reg1_extra = 0x08;
    v.reg11 = 0x03;
    v.reg12 = 0x81;
    break;
  case 7: // left column blank
    v.reg0_extra = 0x20;
    v.reg11 = 0x03;
    break;
  case 8: // window above and left
    v.reg17 = 0x05;
    v.reg18 = 0x03;
    break;
  case 9: // window right and below, H40
    v.reg17 = 0x8A;
    v.reg18 = 0x83;
    v.reg12 = 0x81;
    break;
  case 10: // window left, H40
    v.reg17 = 0x0C;
    v.reg12 = 0x81;
    break;
  case 11: // window right
    v.reg17 = 0x85;
    break;
  case 12: // window below, line scroll
    v.reg18 = 0x8E;
    v.reg11 = 0x03;
    break;
  case 13: // shadow/highlight mode, line scroll
    v.reg12 = 0x08;
    v.reg11 = 0x03;
    break;
  case 14: // 64x64 only
    v.reg16 = 0x11;
    break;
  case 15: // 2 cell vertical scroll only
    v.reg11 = 0x04;
    break;
  case 16: // 2 cell vertical scroll, H40
    v.reg11 = 0x04;
    v.reg12 = 0x81;
    break;
  case 17: // left window, plane A fine scroll 1 (even column)
  case 18: // fine scroll 1, odd column
  case 19: // fine scroll 4
  case 20: // fine scroll 7
    v.reg17 = 0x05;
    v.hscroll_a = id == 17 ? 49 : id == 18 ? 41 : id == 19 ? 52 : 55;
    break;
  case 21: // limited palette with shadow/highlight
    v.limited_palette = true;
    v.reg12 = 0x08;
    break;
  case 22: // interlace mode 1
    v.reg12 = 0x02;
    break;
  case 23: // interlace mode 2 (double resolution)
    v.reg12 = 0x06;
    break;
  case 24: // interlace mode 2, H40, line scroll
    v.reg12 = 0x87;
    v.reg11 = 0x03;
    break;
  case 25: // interlace mode 2 with 2 cell vertical scroll
    v.reg12 = 0x06;
    v.reg11 = 0x04;
    break;
  case 26: // interlace mode 2 with the window
    v.reg12 = 0x06;
    v.reg17 = 0x05;
    v.reg18 = 0x03;
    break;
  case 27: // interlace mode 2, window right and below, shadow
    v.reg12 = 0x0E;
    v.reg17 = 0x85;
    v.reg18 = 0x84;
    break;
  case 28: // H32 and V30
    v.reg1_extra = 0x08;
    break;
  case 29: // H40 and V28, left column blank
    v.reg12 = 0x81;
    v.reg0_extra = 0x20;
    break;
  case 30: // H40 and V30, 64x64 planes, cell scroll
    v.reg1_extra = 0x08;
    v.reg12 = 0x81;
    v.reg16 = 0x11;
    v.reg11 = 0x02;
    break;
  case 31: // H32 and V30, window below
    v.reg1_extra = 0x08;
    v.reg18 = 0x8C;
    break;
  case 32: // window right and below, shadow (27 without interlace)
    v.reg12 = 0x08;
    v.reg17 = 0x85;
    v.reg18 = 0x84;
    break;
  case 33: // window above and left, shadow, line scroll
    v.reg12 = 0x08;
    v.reg11 = 0x03;
    v.reg17 = 0x05;
    v.reg18 = 0x03;
    break;
  default:
    break;
  }
  return v;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `sprite_limits` variant: random but deterministic sprites over random
// planes. `count` sprites chained in the table (link order = index order),
// each up to `max_cells` cells per side, so a variant stays inside or crosses
// the per line limits as chosen.
struct ExplicitSprite {
  int x = 0, y = 0; // screen position (raw table value is +128)
  unsigned w = 1, h = 1;
  std::uint16_t attr = 1; // tile, flips, palette and priority
};

struct SpriteVariant {
  std::vector<ExplicitSprite> sprites; // when not empty: these, in order
  unsigned count = 8;
  unsigned max_cells = 2;    // 1..4 cells per side
  std::uint8_t reg12 = 0x00; // 0x81 for H40
  unsigned seed = 1;
  bool planes = true;       // draw both planes under the sprites
  bool x_zero = false;      // some sprites with raw X = 0 (mask)
  bool dense = false;       // pack the sprites into a few lines (limits)
  bool custom_tile = false; // tile 127: left half opaque, right half clear
  unsigned shade_probe = 0; // 1: sprite x plane priority/colour matrix
  bool col7_tile = false;   // tile 127: only column 7 opaque
  bool col0_tile = false;   // tile 127: only column 0 opaque
  bool row0_tile = false;   // tile 127: only the right half of row 0 opaque
  unsigned fixed_w = 0;     // cells wide for every sprite (0 = random)
  unsigned fixed_h = 0;     // cells tall (0 = random)
  bool poll_status = false; // read the sprite flags into work RAM
  std::uint8_t reg1_extra = 0x00;
  std::uint8_t reg0_extra = 0x00; // 0x20: left column blank
};

inline std::vector<std::uint8_t> sprite_limits_rom(const SpriteVariant &v) {
  Scene scene;
  scene.regs = {{2, 0x10},  {4, 0x06},  {3, 0x34},  {13, 0x38},
                {5, 0x70},  {11, 0x00}, {16, 0x00}, {12, v.reg12},
                {17, 0x00}, {18, 0x00}, {7, 0x05}};
  scene.reg1 = static_cast<std::uint8_t>(0x44 | v.reg1_extra);
  if (v.reg0_extra != 0)
    scene.regs.push_back({0, static_cast<std::uint8_t>(0x04 | v.reg0_extra)});
  scene.poll_status = v.poll_status;
  Lcg rng{0x5EED1000U + v.seed};
  std::vector<std::uint16_t> tiles;
  for (unsigned t = 0; t < 128; ++t)
    for (unsigned row = 0; row < 8; ++row) {
      std::uint32_t word = 0;
      for (unsigned col = 0; col < 8; ++col) {
        const unsigned colour =
            (t == 0) ? 0U : (((t * 7U + row * 3U + col * 5U) % 15U) + 1U);
        const unsigned c = (rng.next() % 4U == 0U) ? 0U : colour;
        word = (word << 4) | c;
      }
      tiles.push_back(static_cast<std::uint16_t>(word >> 16));
      tiles.push_back(static_cast<std::uint16_t>(word));
    }
  scene.vram.emplace_back(0x0000, tiles);
  if (v.custom_tile) {
    std::vector<std::uint16_t> custom;
    for (unsigned row = 0; row < 16; ++row) {
      custom.push_back(0x1234);
      custom.push_back(0x0000);
    }
    scene.vram.emplace_back(127U * 32U, custom);
  }
  if (v.col7_tile) {
    std::vector<std::uint16_t> custom;
    for (unsigned row = 0; row < 8; ++row) {
      custom.push_back(0x0000);
      custom.push_back(0x0001);
    }
    scene.vram.emplace_back(127U * 32U, custom);
  }
  if (v.col0_tile) {
    std::vector<std::uint16_t> custom;
    for (unsigned row = 0; row < 8; ++row) {
      custom.push_back(0x1000);
      custom.push_back(0x0000);
    }
    scene.vram.emplace_back(127U * 32U, custom);
  }
  if (v.row0_tile) {
    std::vector<std::uint16_t> custom(16, 0);
    custom[1] = 0x1234;
    scene.vram.emplace_back(127U * 32U, custom);
  }
  std::vector<std::uint16_t> plane(32 * 32);
  for (auto &e : plane) {
    const std::uint32_t r = rng.next();
    e = v.planes ? static_cast<std::uint16_t>(((r & 0x100U) ? 0x8000U : 0U) |
                                              (((r >> 9) & 3U) << 13) |
                                              ((r >> 12) % 64U))
                 : 0;
  }
  scene.vram.emplace_back(0x4000, plane);
  for (auto &e : plane) {
    const std::uint32_t r = rng.next();
    e = v.planes ? static_cast<std::uint16_t>(((r & 0x100U) ? 0x8000U : 0U) |
                                              (((r >> 9) & 3U) << 13) |
                                              ((r >> 12) % 64U))
                 : 0;
  }
  scene.vram.emplace_back(0xC000, plane);
  SpriteVariant pv = v;
  if (v.shade_probe != 0) {
    // tiles 124 to 127: solid colours 5, 14, 15 and 3
    std::vector<std::uint16_t> solid;
    for (const unsigned colour : {5U, 14U, 15U, 3U})
      for (unsigned i = 0; i < 16; ++i)
        solid.push_back(static_cast<std::uint16_t>(colour * 0x1111U));
    scene.vram.emplace_back(124U * 32U, solid);
    std::vector<std::uint16_t> cells(32 * 32, 0);
    unsigned i = 0;
    for (const unsigned sprite_attr_kind : {0U, 1U, 2U, 3U}) // see below
      for (const unsigned sprite_prio : {0U, 1U})
        for (const unsigned plane_prio : {0U, 1U}) {
          const unsigned tile = sprite_attr_kind == 0   ? 124U  // pal 0 col 5
                                : sprite_attr_kind == 1 ? 124U  // pal 3 col 5
                                : sprite_attr_kind == 2 ? 125U  // pal 3 col 14
                                                        : 126U; // col 15
          const unsigned pal = sprite_attr_kind == 0 ? 0U : 3U;
          cells[5 * 32 + 2 * i] =
              static_cast<std::uint16_t>((plane_prio ? 0x8000U : 0U) | 127U);
          pv.sprites.push_back(
              {static_cast<int>(16 * i), 40, 1, 1,
               static_cast<std::uint16_t>((sprite_prio ? 0x8000U : 0U) |
                                          (pal << 13) | tile)});
          ++i;
        }
    if (v.shade_probe == 2) {
      // no plane tiles: palette 0-3 x colour 5/14/15 x priority over the
      // backdrop (tile 124 colour 5, 125 colour 14, 126 colour 15)
      pv.sprites.clear();
      std::fill(cells.begin(), cells.end(), 0);
      unsigned k = 0;
      for (unsigned pal = 0; pal < 4; ++pal)
        for (unsigned tile = 124; tile <= 126; ++tile)
          for (unsigned prio = 0; prio < 2; ++prio, ++k)
            pv.sprites.push_back(
                {static_cast<int>(16 * (k % 12)),
                 static_cast<int>(40 + 40 * (k / 12)), 1, 1,
                 static_cast<std::uint16_t>((prio ? 0x8000U : 0U) |
                                            (pal << 13) | tile)});
    }
    scene.vram.emplace_back(0xC000, cells);
  }
  const SpriteVariant &v2 = pv;
  const bool h40 = (v.reg12 & 0x81U) != 0;
  const unsigned width = h40 ? 320U : 256U;
  std::vector<std::uint16_t> sat;
  for (unsigned n = 0; n < v2.sprites.size(); ++n) {
    const auto &e = v2.sprites[n];
    const unsigned link = n + 1U < v2.sprites.size() ? n + 1U : 0U;
    sat.push_back(static_cast<std::uint16_t>(e.y + 128));
    sat.push_back(static_cast<std::uint16_t>(((e.w - 1U) << 10) |
                                             ((e.h - 1U) << 8) | link));
    sat.push_back(e.attr);
    sat.push_back(static_cast<std::uint16_t>(e.x + 128));
  }
  for (unsigned n = 0; n < v.count && v2.sprites.empty(); ++n) {
    unsigned w = rng.next() % v.max_cells + 1U;
    unsigned h = rng.next() % v.max_cells + 1U;
    if (v.fixed_w != 0)
      w = v.fixed_w;
    if (v.fixed_h != 0)
      h = v.fixed_h;
    unsigned y = 128U + rng.next() % 240U - 16U;
    unsigned x = 128U + rng.next() % (width + 16U) - 16U;
    if (v.dense) {
      y = 128U + 40U + rng.next() % 3U;
      x = 128U + rng.next() % width;
    }
    if (v.x_zero && n % 5U == 3U)
      x = 0;
    const std::uint32_t r = rng.next();
    const unsigned link = n + 1U < v.count ? n + 1U : 0U;
    sat.push_back(static_cast<std::uint16_t>(y));
    sat.push_back(
        static_cast<std::uint16_t>(((w - 1U) << 10) | ((h - 1U) << 8) | link));
    sat.push_back(static_cast<std::uint16_t>(
        ((r & 0x100U) ? 0x8000U : 0U) | (((r >> 9) & 3U) << 13) |
        ((r & 0x200U) ? 0x1000U : 0U) | ((r & 0x400U) ? 0x0800U : 0U) |
        ((r >> 12) % (128U - w * h))));
    sat.push_back(static_cast<std::uint16_t>(x));
  }
  scene.vram.emplace_back(0xE000, sat);
  scene.cram.resize(64);
  for (auto &c : scene.cram)
    c = static_cast<std::uint16_t>(rng.next() & 0x0EEEU);
  scene.vsram.assign(40, 0);
  return scene_rom(scene, "AYTHER SPRITELIM");
}

/// Variant `id` of `sprite_limits`; mirrored in tools/diffbench/CMakeLists.txt.
inline SpriteVariant sprite_variant(unsigned id) {
  SpriteVariant v;
  v.seed = id;
  switch (id) {
  case 0: // few sprites, H32, planes under
    v.count = 6;
    break;
  case 1: // H40, bigger sprites
    v.count = 8;
    v.max_cells = 3;
    v.reg12 = 0x81;
    break;
  case 2: // no planes: sprites alone over the backdrop
    v.count = 10;
    v.planes = false;
    break;
  case 3: // 1 cell sprites, many
    v.count = 40;
    v.max_cells = 1;
    break;
  case 4: // V30
    v.count = 8;
    v.reg1_extra = 0x08;
    break;
  case 5: // more sprites than the line count limit, H32 (16)
    v.count = 25;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    break;
  case 6: // pixel limit: 10 sprites of 32 pixels, H32 (256)
    v.count = 10;
    v.fixed_w = 4;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    break;
  case 7: // X = 0 masking
    v.count = 12;
    v.max_cells = 2;
    v.dense = true;
    v.x_zero = true;
    v.poll_status = true;
    break;
  case 8: // collision between a few overlapping sprites
    v.count = 5;
    v.dense = true;
    v.poll_status = true;
    break;
  case 9: // line count limit in H40 (20)
    v.count = 24;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.reg12 = 0x81;
    v.poll_status = true;
    break;
  case 10: // pixel limit in H40 (320): 12 sprites of 32 pixels
    v.count = 12;
    v.fixed_w = 4;
    v.fixed_h = 1;
    v.dense = true;
    v.reg12 = 0x81;
    v.poll_status = true;
    break;
  case 11: // more sprites than the frame limit, H32 (64)
    v.count = 72;
    v.max_cells = 1;
    v.poll_status = true;
    break;
  case 12: // no overlap at all: flags stay clear
    v.count = 3;
    v.max_cells = 1;
    v.poll_status = true;
    break;
  case 13:
    v.count = 1;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    break;
  case 14:
    v.count = 2;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    break;
  case 15:
    v.count = 3;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    break;
  case 16:
    v.count = 4;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    break;
  case 17: // one sprite cut by the right edge, alone
    v.poll_status = true;
    v.planes = false;
    v.sprites = {{252, 40, 1, 2, 0x0F}};
    break;
  case 18: // one sprite cut by the left edge, alone
    v.poll_status = true;
    v.planes = false;
    v.sprites = {{-4, 40, 1, 2, 0x0F}};
    break;
  case 19: // two sprites that do not overlap, one cut by the right edge
    v.poll_status = true;
    v.planes = false;
    v.sprites = {{252, 42, 1, 1, 0x0F}, {100, 42, 2, 1, 0x4B}};
    break;
  case 20: // the same two with the cut one on the screen
    v.poll_status = true;
    v.planes = false;
    v.sprites = {{200, 42, 1, 1, 0x0F}, {100, 42, 2, 1, 0x4B}};
    break;
  case 21: // cut by the right edge, the part outside is clear
    v.sprites = {{252, 40, 1, 1, 0x7F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 22: // cut by the right edge, the part outside is opaque (h flip)
    v.sprites = {{252, 40, 1, 1, 0x087F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 23: // cut by the left edge, the part outside is opaque
    v.sprites = {{-4, 40, 1, 1, 0x7F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 24: // cut by the left edge, the part outside is clear (h flip)
    v.sprites = {{-4, 40, 1, 1, 0x087F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 25: // one line of opaque pixels outside the right edge
    v.sprites = {{252, 40, 1, 1, 0x7F}};
    v.row0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 100:
  case 101:
  case 102:
  case 103:
  case 104:
  case 105:
  case 106:
  case 107:
  case 108:
  case 109:
  case 110:
  case 111:
  case 112:
  case 113:
  case 114:
  case 115:
  case 116:
  case 117:
  case 118:
  case 119:
  case 120: {
    // one sprite, column 0 only, at a chosen X: how far outside the screen
    // do pixels count for the collision flag
    static const int xs[] = {255, 256, 257, 258, 260, 264,  270,
                             280, 300, 340, 383, 384, -1,   -2,
                             -8,  -9,  -16, -32, -64, -100, -128};
    v.sprites = {{xs[id - 100], 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  }
  case 130:
  case 131:
  case 132:
  case 133:
  case 134:
  case 135:
    // column 7 only, at X 248 to 253: pixels 255 to 260
    v.sprites = {{248 + static_cast<int>(id - 130), 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 140:
  case 141:
  case 142:
  case 143:
  case 144:
  case 145:
    // column 0 only, at X -3 to -8
    v.sprites = {{-3 - static_cast<int>(id - 140), 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 150: // 131 with planes
    v.sprites = {{249, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.poll_status = true;
    break;
  case 151: // 16 without planes
    v.count = 4;
    v.fixed_w = 1;
    v.fixed_h = 1;
    v.dense = true;
    v.poll_status = true;
    v.planes = false;
    v.seed = 16;
    break;
  case 152: // 131 with a second sprite on the same lines
    v.sprites = {{249, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 153: // 131 with a second sprite before it
    v.sprites = {{100, 40, 1, 1, 0x7F}, {249, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 154:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x0F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 155:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {100, 100, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 156:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {100, 48, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 157:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {100, 40, 2, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 158:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {300, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 159:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {-4, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 160:
    v.sprites = {{249, 40, 1, 1, 0x7F}, {100, 44, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 161:
    v.sprites = {{252, 40, 1, 1, 0x087F}, {100, 40, 1, 1, 0x7F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 162:
    v.sprites = {{100, 40, 1, 1, 0x7F}, {252, 40, 1, 1, 0x087F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 163:
    v.sprites = {{252, 40, 1, 1, 0x087F}, {252, 50, 1, 1, 0x087F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 164:
    v.sprites = {{249, 40, 1, 1, 0x087F}, {100, 40, 1, 1, 0x7F}};
    v.custom_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 165:
    v.sprites = {{250, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 166:
    v.sprites = {{251, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 167:
    v.sprites = {{252, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col7_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 170:
    v.sprites = {{-1, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 171:
    v.sprites = {{-2, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 172:
    v.sprites = {{-3, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 173:
    v.sprites = {{-4, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 174:
    v.sprites = {{-5, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 175:
    v.sprites = {{-6, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 176:
    v.sprites = {{-7, 40, 1, 1, 0x7F}, {100, 40, 1, 1, 0x7F}};
    v.col0_tile = true;
    v.planes = false;
    v.poll_status = true;
    break;
  case 180:
    v.sprites = {
        {0, 40, 1, 1, 0x0F}, {50, 40, 1, 1, 0x1F}, {90, 40, 1, 1, 0x2F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 181:
    v.sprites = {{20, 40, 1, 1, 0x0F},
                 {0, 40, 1, 1, 0x1F},
                 {90, 40, 1, 1, 0x2F},
                 {130, 44, 1, 1, 0x3F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 182:
    v.sprites = {
        {20, 40, 1, 1, 0x0F}, {0, 100, 1, 1, 0x1F}, {90, 100, 1, 1, 0x2F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 183:
    v.sprites = {
        {-100, 40, 1, 1, 0x0F}, {0, 40, 1, 1, 0x1F}, {90, 40, 1, 1, 0x2F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 184:
    v.sprites = {
        {20, 40, 1, 1, 0x0F}, {-128, 40, 1, 1, 0x1F}, {90, 40, 1, 1, 0x2F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 185:
    v.sprites = {{20, 40, 1, 1, 0x0F},
                 {0, 40, 1, 1, 0x1F},
                 {0, 40, 1, 1, 0x2F},
                 {90, 40, 1, 1, 0x3F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 186:
    v.sprites = {
        {300, 40, 1, 1, 0x0F}, {300, 40, 1, 1, 0x10}, {300, 40, 1, 1, 0x11},
        {300, 40, 1, 1, 0x12}, {300, 40, 1, 1, 0x13}, {300, 40, 1, 1, 0x14},
        {300, 40, 1, 1, 0x15}, {300, 40, 1, 1, 0x16}, {300, 40, 1, 1, 0x17},
        {300, 40, 1, 1, 0x18}, {300, 40, 1, 1, 0x19}, {300, 40, 1, 1, 0x1A},
        {300, 40, 1, 1, 0x1B}, {300, 40, 1, 1, 0x1C}, {300, 40, 1, 1, 0x1D},
        {300, 40, 1, 1, 0x1E}, {20, 40, 1, 1, 0x1F},  {40, 40, 1, 1, 0x2F},
        {60, 40, 1, 1, 0x3F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 187:
    v.sprites = {{300, 40, 4, 1, 0x0F}, {300, 40, 4, 1, 0x10},
                 {300, 40, 4, 1, 0x11}, {300, 40, 4, 1, 0x12},
                 {300, 40, 4, 1, 0x13}, {300, 40, 4, 1, 0x14},
                 {300, 40, 4, 1, 0x15}, {300, 40, 4, 1, 0x16},
                 {20, 40, 2, 1, 0x1F},  {80, 40, 1, 1, 0x2F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 188:
    v.sprites = {
        {20, 40, 1, 1, 0x0F}, {20, 40, 1, 1, 0x10}, {20, 40, 1, 1, 0x11},
        {20, 40, 1, 1, 0x12}, {20, 40, 1, 1, 0x13}, {20, 40, 1, 1, 0x14},
        {20, 40, 1, 1, 0x15}, {20, 40, 1, 1, 0x16}, {20, 40, 1, 1, 0x17},
        {20, 40, 1, 1, 0x18}, {20, 40, 1, 1, 0x19}, {20, 40, 1, 1, 0x1A},
        {20, 40, 1, 1, 0x1B}, {20, 40, 1, 1, 0x1C}, {20, 40, 1, 1, 0x1D},
        {60, 40, 1, 1, 0x1F}, {70, 40, 1, 1, 0x2F}};
    v.planes = false;
    v.poll_status = true;
    break;
  case 200: // shadow/highlight with random sprites over random planes
  case 201:
  case 202:
  case 203:
    v.count = 12;
    v.max_cells = 2;
    v.reg12 = 0x08;
    v.seed = id;
    v.poll_status = true;
    break;
  case 205:
  case 206:
  case 207:
  case 208:
  case 209: // more seeds, dense sprites so that they overlap
    v.count = 14;
    v.max_cells = 2;
    v.reg12 = 0x08;
    v.dense = (id & 1U) != 0;
    v.seed = id;
    v.poll_status = true;
    break;
  case 204: // same in H40
    v.count = 16;
    v.max_cells = 3;
    v.reg12 = 0x89;
    v.seed = id;
    v.poll_status = true;
    break;
  case 210: // shadow/highlight matrix: sprite kind x priorities
    v.shade_probe = 1;
    v.reg12 = 0x08;
    v.planes = false;
    v.poll_status = true;
    break;
  case 211: // shadow/highlight: sprite palette x colour over the backdrop
    v.shade_probe = 2;
    v.reg12 = 0x08;
    v.planes = false;
    v.poll_status = true;
    break;
  case 220: // interlace
    v.count = 10;
    v.max_cells = 2;
    v.reg12 = 0x06;
    v.seed = id;
    v.poll_status = true;
    break;
  case 221: // interlace
    v.count = 12;
    v.max_cells = 3;
    v.reg12 = 0x87;
    v.seed = id;
    v.poll_status = true;
    break;
  case 222: // interlace
    v.count = 12;
    v.max_cells = 2;
    v.reg12 = 0x0E;
    v.seed = id;
    v.poll_status = true;
    break;
  case 223: // interlace
    v.count = 10;
    v.max_cells = 2;
    v.reg12 = 0x02;
    v.seed = id;
    v.poll_status = true;
    break;
  case 224: // left column blank over sprites (LAYER_DIM, TP-205.2)
    v.count = 30;
    v.max_cells = 3;
    v.reg0_extra = 0x20;
    break;
  case 225: // the same with shadow/highlight, H40
    v.count = 30;
    v.max_cells = 3;
    v.reg12 = 0x89;
    v.reg0_extra = 0x20;
    break;
  default:
    break;
  }
  return v;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `mode4_scene`: a Master System style scene through the 68000 side of the
// VDP (Power Base Converter): register 15 is set to 1 in Mode 5 so that every
// word write steps 2 bytes, then the VDP goes to Mode 4 (register 1 bit 2
// clear) and every control word is a whole command. Random tiles, name table,
// sprites and CRAM, deterministic.
struct Mode4Variant {
  std::uint8_t reg0 = 0x04; // bit 5 left column, 6 lock top rows, 7 lock right
  std::uint8_t reg1 = 0x40; // bit 0 zoom, bit 1 8x16 sprites (display on)
  std::uint8_t reg6 = 0x00; // bit 2: sprite tiles from 256
  std::uint8_t reg7 = 0x00;
  std::uint8_t reg8 = 0x00; // horizontal scroll
  std::uint8_t reg9 = 0x00; // vertical scroll
  unsigned sprites = 8;
  unsigned seed = 1;
};

/// Variant `id` of `mode4_scene`; mirrored in tools/diffbench/CMakeLists.txt.
inline Mode4Variant mode4_variant(unsigned id) {
  Mode4Variant v;
  v.seed = id;
  switch (id) {
  case 1:
    break;
  case 2: // scrolls with the left column blank (gpgx draws stale pixels
          // in the first reg8 & 7 columns otherwise)
    v.reg0 = 0x04 | 0x20;
    v.reg8 = 37;
    v.reg9 = 53;
    break;
  case 3: // scroll locks and left column
    v.reg0 = 0x04 | 0x20 | 0x40 | 0x80;
    v.reg8 = 21;
    v.reg9 = 77;
    break;
  case 4: // 8x16 sprites
    v.reg1 = 0x42;
    v.sprites = 10;
    break;
  case 5: // zoomed sprites
    v.reg1 = 0x41;
    break;
  case 6: // sprite tiles from 256, backdrop colour 5
    v.reg6 = 0x04;
    v.reg7 = 0x05;
    break;
  case 7: // sprites shifted left by 8
    v.reg0 = 0x04 | 0x08;
    break;
  case 8: // more than 8 sprites on a line (overflow)
    v.sprites = 40;
    break;
  case 9: // 64 sprites, 8x16
    v.reg1 = 0x42;
    v.sprites = 64;
    break;
  case 10: // display off
    v.reg1 = 0x00;
    v.reg7 = 0x03;
    break;
  case 11: // scroll wrap on both axes, left column
    v.reg0 = 0x04 | 0x20;
    v.reg8 = 250;
    v.reg9 = 200;
    break;
  default:
    break;
  }
  return v;
}

inline std::vector<std::uint8_t> mode4_scene_rom(const Mode4Variant &v) {
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0000, // lea $C00000,a0 (data port)
      0x43F9, 0x00C0, 0x0004, // lea $C00004,a1 (control port)
  };
  const auto reg = [&](std::uint8_t r, std::uint8_t value) {
    c.insert(c.end(),
             {0x32BC, static_cast<std::uint16_t>(0x8000 | (r << 8) | value)});
  };
  const auto command = [&](std::uint16_t w) { c.insert(c.end(), {0x32BC, w}); };
  const auto data = [&](std::uint16_t w) { c.insert(c.end(), {0x30BC, w}); };
  reg(0, 0x04);
  reg(1, 0x04); // Mode 5, display off
  reg(15, 1);
  reg(1, 0x00); // Mode 4, display off
  reg(0, v.reg0);
  reg(2, 0x0E); // name table at $3800
  reg(5, 0x7E); // sprite table at $3F00
  reg(6, v.reg6);
  reg(7, v.reg7);
  reg(8, v.reg8);
  reg(9, v.reg9);
  reg(10, 0xFF);
  Lcg rng{0x5EED4000U + v.seed};
  // tiles 0..63 at $0000 (and 256..319 at $2000): 32 bytes of planar data
  for (unsigned base : {0x0000U, 0x2000U}) {
    command(static_cast<std::uint16_t>(0x4000 | base));
    for (unsigned t = 0; t < 64; ++t)
      for (unsigned i = 0; i < 16; ++i)
        data(static_cast<std::uint16_t>(rng.next() & 0xFFFFU));
  }
  // name table: 32x28 entries. The low byte of an entry comes first in
  // VRAM, i.e. in the high half of the word written: tile low, then flags
  // (bit 0 tile bit 8, 1 h flip, 2 v flip, 3 palette, 4 priority).
  command(0x7800);
  for (unsigned i = 0; i < 32 * 28; ++i) {
    const std::uint32_t r = rng.next();
    data(static_cast<std::uint16_t>((((r >> 12) % 64U) << 8) |
                                    ((r >> 4) & 0x1FU)));
  }
  // sprite table: 64 Y bytes (0xD0 ends the list), then X and tile pairs
  command(0x7F00);
  std::vector<std::uint8_t> ys(64, 0xD0), xs(128, 0);
  for (unsigned n = 0; n < v.sprites && n < 64; ++n) {
    ys[n] = static_cast<std::uint8_t>(rng.next() % 180U);
    xs[2 * n] = static_cast<std::uint8_t>(rng.next() % 256U);
    xs[2 * n + 1] = static_cast<std::uint8_t>(rng.next() % 64U);
  }
  for (unsigned i = 0; i < 64; i += 2)
    data(static_cast<std::uint16_t>((ys[i] << 8) | ys[i + 1]));
  for (unsigned i = 0; i < 128; i += 2)
    data(static_cast<std::uint16_t>((xs[i] << 8) | xs[i + 1]));
  // CRAM: one command per entry (a word write steps 2 entries otherwise)
  for (unsigned i = 0; i < 32; ++i) {
    command(static_cast<std::uint16_t>(0xC000 | i));
    data(static_cast<std::uint16_t>(rng.next() & 0x3FU));
  }
  reg(1, v.reg1);
  c.push_back(0x60FE);
  std::vector<std::uint8_t> rom(0x200 + c.size() * 2 + 2, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER MODE4SCEN", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `fm_psg_sweep` (TP-107.2/107.3): with the Z80 bus granted the 68000 repeats
// once per frame (synchronised on the start of the vertical blanking) a list of
// writes to the YM2612 ($A04000-3) and to the PSG ($C00010-7): the observation
// region AUDIO_WRITES of the last frame is what the bench compares. Variants:
//   0 FM: ports, parts, address-only and data-only writes, DAC, key-on
//   1 PSG: byte writes on the four odd addresses, word writes, even bytes
//   2 gaps: the same data write after 0..12 nops, then after growing delays
//   3 FM words: word writes on the chip ports
//   4 flood: 9 000 PSG writes in a row (more than the log holds)
//   5 audible: FM channels 1 and 2 (algorithm 7, fast attack) and the PSG
//     tones 0 and 1 and the noise, all at full volume (TP-107.5)
class Ops68 {
public:
  // move.b #v,off(a0) / move.b #v,off(a1) / move.b #v,off(a2)
  void ym(unsigned off, unsigned v) {
    c.insert(c.end(), {0x117C, w(v), w(off)});
  }
  void psg(unsigned off, unsigned v) {
    c.insert(c.end(), {0x137C, w(v), w(off)});
  }
  // move.b #v,(a1): 12 cycles, the fastest way to fill the log
  void psg_fast(unsigned v) { c.insert(c.end(), {0x12BC, w(v)}); }
  void psg_even(unsigned off, unsigned v) {
    c.insert(c.end(), {0x157C, w(v), w(off)});
  }
  // move.w #v,off(a0) / off(a2)
  void ymw(unsigned off, unsigned v) {
    c.insert(c.end(), {0x317C, w(v), w(off)});
  }
  void psgw(unsigned off, unsigned v) {
    c.insert(c.end(), {0x357C, w(v), w(off)});
  }
  void nops(unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      c.push_back(0x4E71);
  }
  // move.w #n,d0; dbra d0,*: n + 1 iterations of 10 cycles
  void delay(unsigned n) { c.insert(c.end(), {0x303C, w(n), 0x51C8, 0xFFFE}); }
  std::vector<std::uint16_t> c;

private:
  static std::uint16_t w(unsigned v) { return static_cast<std::uint16_t>(v); }
};

//   `once`: no synchronisation; after a fixed delay the list runs one time and
//   the 68000 stops, so it all happens in frame 0 (the frame start, not the
//   status of the VDP, fixes the cycles)
inline std::vector<std::uint8_t> fm_psg_sweep_rom(unsigned variant,
                                                  bool once = false) {
  Ops68 o;
  switch (variant) {
  case 0:
    o.ym(0, 0x22);
    o.ym(1, 0x08); // part 1: address, data
    o.ym(0, 0x27);
    o.ym(1, 0x00);
    o.ym(0, 0x28);
    o.ym(1, 0xF0); // key on
    o.ym(1, 0x55); // data again, no address
    o.ym(0, 0x30); // address alone
    o.ym(2, 0x1A);
    o.ym(3, 0x77); // part 2: address, data
    o.ym(3, 0x66);
    o.ym(0, 0x2A);
    o.ym(1, 0x80); // DAC data
    o.ym(0, 0x2B);
    o.ym(1, 0x80); // DAC enable
    o.ym(0, 0xB4);
    o.ym(1, 0xC0);
    o.delay(100);
    o.ym(0, 0xA4);
    o.ym(1, 0x22);
    o.ym(0, 0xA0);
    o.ym(1, 0x69);
    o.ym(2, 0xB4);
    o.ym(3, 0xC0);
    o.delay(1000);
    o.ym(0, 0x28);
    o.ym(1, 0x00); // key off
    o.ym(0, 0x21);
    o.ym(1, 0x01); // test register
    o.ym(0, 0x24);
    o.ym(1, 0x80);
    o.ym(0, 0x25);
    o.ym(1, 0x02);
    o.ym(0, 0x27);
    o.ym(1, 0x15); // timer A load
    break;
  case 1:
    o.psg(0, 0x8F);
    o.psg(0, 0x3F); // $C00011: tone 0 low, data
    o.psg(2, 0x90); // $C00013: attenuation
    o.psg(4, 0xA5);
    o.psg(6, 0xB3); // $C00015, $C00017
    o.psg(0, 0xE5);
    o.psg(0, 0xF1);      // noise, attenuation
    o.psg_even(0, 0xC9); // byte on $C00010: high lane
    o.psg_even(2, 0xD8); // $C00012
    o.psgw(0, 0x00A7);   // word on $C00010
    o.psgw(2, 0x1B84);   // word on $C00012
    o.psgw(4, 0xFFC2);
    o.psgw(6, 0x0001);
    o.psg(0, 0x9F);
    break;
  case 2:
    for (unsigned n = 0; n <= 12; ++n) {
      o.ym(1, n);
      o.nops(n);
    }
    for (unsigned d : {0u, 1u, 2u, 5u, 10u, 40u, 120u, 400u}) {
      o.ym(1, 0xE0 + d % 16);
      o.delay(d);
    }
    for (unsigned n = 0; n <= 6; ++n) {
      o.psg(0, 0x80 + n);
      o.nops(n);
    }
    break;
  case 4:
    for (unsigned n = 0; n < 9000; ++n)
      o.psg_fast(0x80 + (n & 0x1F));
    break;
  case 5:
    for (unsigned ch = 0; ch < 2; ++ch) {
      o.ym(0, 0xB0 + ch);
      o.ym(1, 0x07); // algorithm 7: all carriers
      o.ym(0, 0xB4 + ch);
      o.ym(1, 0xC0); // both speakers
      for (unsigned op = 0; op < 4; ++op) {
        o.ym(0, 0x30 + 4 * op + ch);
        o.ym(1, 0x01); // multiple 1
        o.ym(0, 0x40 + 4 * op + ch);
        o.ym(1, 0x00); // total level 0
        o.ym(0, 0x50 + 4 * op + ch);
        o.ym(1, 0x1F); // attack rate 31
        o.ym(0, 0x60 + 4 * op + ch);
        o.ym(1, 0x00);
        o.ym(0, 0x70 + 4 * op + ch);
        o.ym(1, 0x00);
        o.ym(0, 0x80 + 4 * op + ch);
        o.ym(1, 0x0F);
      }
      o.ym(0, 0xA4 + ch);
      o.ym(1, 0x22 + ch);
      o.ym(0, 0xA0 + ch);
      o.ym(1, 0x69 + 16 * ch);
      o.ym(0, 0x28);
      o.ym(1, 0xF0 + ch); // key on, all operators
    }
    o.psg(0, 0x8A);
    o.psg(0, 0x0C);
    o.psg(0, 0x90); // tone 0
    o.psg(0, 0xA5);
    o.psg(0, 0x07);
    o.psg(0, 0xB0); // tone 1
    o.psg(0, 0xE4);
    o.psg(0, 0xF0); // white noise, loudest
    break;
  default:
    o.ymw(0, 0x2208); // high byte $22, low byte $08
    o.ym(1, 0x99);    // data: which register did the word latch?
    o.ymw(0, 0x2700);
    o.ym(1, 0x98);
    o.ymw(2, 0x1A55);
    o.ym(3, 0x97); // part 2
    o.ymw(2, 0x30AA);
    o.ym(3, 0x96);
    o.ymw(4, 0x2B80); // $A04004: a mirror of the ports?
    o.ym(5, 0x95);
    break;
  }
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00A0, 0x4000, // lea $A04000,a0
      0x43F9, 0x00C0, 0x0011, // lea $C00011,a1
      0x45F9, 0x00C0, 0x0010, // lea $C00010,a2
      0x4DF9, 0x00C0, 0x0004, // lea $C00004,a6
      0x49F9, 0x00A1, 0x1100, // lea $A11100,a4
      0x4BF9, 0x00A1, 0x1200, // lea $A11200,a5
      0x3ABC, 0x0100,         // reset released
      0x38BC, 0x0100,         // bus request
      0x0814, 0x0000, 0x66FA, // wait for the grant
      0x3CBC, 0x8144,         // display on: status bit 3 is the blanking only
  };
  const std::size_t loop = c.size();
  if (once) {
    c.insert(c.end(), {0x303C, 0x0300, 0x51C8, 0xFFFE}); // delay
    c.insert(c.end(), o.c.begin(), o.c.end());
    c.push_back(0x60FE);
  } else {
    c.insert(c.end(), {0x082E, 0x0003, 0x0001, 0x67F8}); // wait for vblank
    c.insert(c.end(), o.c.begin(), o.c.end());
    c.insert(c.end(), {0x082E, 0x0003, 0x0001, 0x66F8}); // wait for its end
    // jmp loop (absolute: bra.w would not reach back over a long list)
    c.insert(c.end(),
             {0x4EF9, 0x0000, static_cast<std::uint16_t>(0x200 + loop * 2)});
  }
  std::vector<std::uint8_t> rom(0x200 + c.size() * 2 + 2, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER FM PSG   ", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `pad_latch` (TP-108.1/108.2): the I/O ports read as the games read them. The
// 68000 writes the control and data registers of the pad ports and stores every
// read of the data registers from $FF0000 in work RAM. The pad is held by the
// harness (a constant mask on port 1 and port 2). Variants:
//   0 three-button protocol: TH output, TH input, partial control bits
//   1 six-button sequence: seven TH toggles on each port, then the same after
//     a pause longer than the pad's timeout
//   2 timeout: a pause of growing length between the counter and its read
//   3 forty TH toggles on both ports (the counter wraps)
//   4 TH changes through the control register (output <-> input)
//   5 the read after TH becomes an input, 0..11 nops later
//   6 the start of 240p Test Suite: port 2 probes, then four TH cycles per port
class PadOps {
public:
  void wc(unsigned port, unsigned v) { w4(0x13FC, v, 0x00A10009 + 2 * port); }
  void wd(unsigned port, unsigned v) { w4(0x13FC, v, 0x00A10003 + 2 * port); }
  void rd(unsigned port) {
    const std::uint32_t a = 0x00A10003 + 2 * port;
    c.insert(c.end(), {0x14F9, static_cast<std::uint16_t>(a >> 16),
                       static_cast<std::uint16_t>(a)});
  }
  void delay(unsigned n) {
    c.insert(c.end(), {0x303C, static_cast<std::uint16_t>(n), 0x51C8, 0xFFFE});
  }
  void nops(unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      c.push_back(0x4E71);
  }
  std::vector<std::uint16_t> c;

private:
  void w4(std::uint16_t op, unsigned v, std::uint32_t a) {
    c.insert(c.end(), {op, static_cast<std::uint16_t>(v & 0xFF),
                       static_cast<std::uint16_t>(a >> 16),
                       static_cast<std::uint16_t>(a)});
  }
};

// `pad_frames` (TP-108.5, risk R-2): at every vertical interrupt the ROM reads
// the data port of both pads with TH high and appends the two bytes to a table
// at $FF0010 (the frame count is at $FF0000). Pad masks that change from one
// frame to the next must show up in the same row on both machines.
inline std::vector<std::uint8_t> pad_frames_rom() {
  auto rom = vint_counter_rom();
  std::memcpy(&rom[0x120], "AYTHER PAD FRAME", 16);
  const std::uint16_t main_code[] = {
      0x23FC, 0x00FF, 0x0010, 0x00FF, 0x000C, // move.l #$FF0010,$FF000C
      0x13FC, 0x0040, 0x00A1, 0x0009,         // move.b #$40,$A10009 (TH out)
      0x13FC, 0x0040, 0x00A1, 0x000B,         // move.b #$40,$A1000B
      0x13FC, 0x0040, 0x00A1, 0x0003,         // move.b #$40,$A10003 (TH high)
      0x13FC, 0x0040, 0x00A1, 0x0005,         // move.b #$40,$A10005
      0x33FC, 0x8164, 0x00C0, 0x0004,         // move.w #$8164,$C00004
      0x46FC, 0x2000,                         // move   #$2000,sr
      0x60FE,                                 // bra.s  .
  };
  std::size_t at = 0x200;
  for (const auto word : main_code) {
    put16(rom, at, word);
    at += 2;
  }
  const std::uint16_t handler[] = {
      0x2079, 0x00FF, 0x000C, // movea.l $FF000C,a0
      0x10F9, 0x00A1, 0x0003, // move.b  $A10003,(a0)+
      0x10F9, 0x00A1, 0x0005, // move.b  $A10005,(a0)+
      0x23C8, 0x00FF, 0x000C, // move.l  a0,$FF000C
      0x52B9, 0x00FF, 0x0000, // addq.l  #1,$FF0000
      0x4E73,                 // rte
  };
  at = 0x300;
  for (const auto word : handler) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

// `region_detect` (TP-108.4): the header's country field (16 bytes at $1F0)
// is set by the caller. The code stores the I/O version register ($A10001) at
// $FF0004 and, on every V interrupt, the number of V counter changes seen
// since the previous one (the lines per frame) at $FF0006. The bench compares
// that RAM with gpgx.
inline std::vector<std::uint8_t>
region_detect_rom(const std::array<std::uint8_t, 16> &country,
                  std::uint16_t checksum = 0) {
  auto rom = vint_counter_rom();
  std::memcpy(&rom[0x120], "AYTHER REGION   ", 16);
  std::memcpy(&rom[0x1F0], country.data(), 16);
  put16(rom, 0x18E, checksum);
  const std::uint16_t main_code[] = {
      0x33FC, 0x8164, 0x00C0, 0x0004,         // move.w #$8164,$C00004
      0x13F9, 0x00A1, 0x0001, 0x00FF, 0x0004, // move.b $A10001,$FF0004
      0x4242,                                 // clr.w  d2
      0x4201,                                 // clr.b  d1
      0x46FC, 0x2000,                         // move   #$2000,sr
      0x1039, 0x00C0, 0x0008,                 // loop: move.b $C00008,d0
      0xB001,                                 // cmp.b  d1,d0
      0x67F6,                                 // beq.s  loop
      0x1200,                                 // move.b d0,d1
      0x5242,                                 // addq.w #1,d2
      0x60F0,                                 // bra.s  loop
  };
  std::size_t at = 0x200;
  for (const auto word : main_code) {
    put16(rom, at, word);
    at += 2;
  }
  const std::uint16_t handler[] = {
      0x33C2, 0x00FF, 0x0006, // move.w d2,$FF0006
      0x4242,                 // clr.w  d2
      0x52B9, 0x00FF, 0x0000, // addq.l #1,$FF0000
      0x4E73,                 // rte
  };
  at = 0x300;
  for (const auto word : handler) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

inline std::vector<std::uint8_t> pad_latch_rom(unsigned variant,
                                               unsigned first = 0,
                                               unsigned last = 3200,
                                               unsigned step = 100) {
  PadOps o;
  switch (variant) {
  case 0:
    o.wc(0, 0x40);
    o.wc(1, 0x40);
    for (unsigned th : {0x40u, 0x00u, 0x40u, 0x00u}) {
      o.wd(0, th);
      o.rd(0);
      o.wd(1, th);
      o.rd(1);
    }
    o.wc(0, 0x00);
    o.wc(1, 0x00); // TH an input: pulled up
    o.rd(0);
    o.rd(1);
    o.wd(0, 0x00);
    o.rd(0); // data written, pin still input
    o.wc(0, 0x7F);
    o.wd(0, 0x7F);
    o.rd(0);
    o.wd(0, 0x3F);
    o.rd(0);
    o.wd(0, 0x55);
    o.rd(0);
    o.wc(0, 0x0F);
    o.wd(0, 0x0A);
    o.rd(0); // pins 0-3 outputs, TH input
    o.wc(0, 0x40);
    o.wd(0, 0x40);
    o.rd(0);
    o.wd(0, 0x00);
    o.rd(0);
    o.wc(0, 0x70);
    o.wd(0, 0x10);
    o.rd(0); // TH output low, bit 4 output high
    break;
  case 1:
    for (unsigned pass = 0; pass < 2; ++pass) {
      o.wc(0, 0x40);
      o.wc(1, 0x40);
      for (unsigned i = 0; i < 9; ++i) {
        const unsigned th = (i & 1U) ? 0x00 : 0x40;
        o.wd(0, th);
        o.rd(0);
        o.wd(1, th);
        o.rd(1);
      }
      o.delay(2000);
    }
    break;
  case 3:
    o.wc(0, 0x40);
    o.wc(1, 0x40);
    for (unsigned i = 0; i < 40;
         ++i) { // enough toggles to see the counter wrap
      const unsigned th = (i & 1U) ? 0x00 : 0x40;
      o.wd(0, th);
      o.rd(0);
      o.wd(1, th);
      o.rd(1);
    }
    break;
  case 5: // when does the pull-up reach the pad?
    for (unsigned k = 0; k < 12; ++k) {
      o.wc(0, 0x40);
      o.wd(0, 0x00);
      o.delay(3000);
      o.wd(0, 0x40);
      o.wd(0, 0x00);
      o.wd(0, 0x40);
      o.wd(0, 0x00);
      o.wc(0, 0x00); // TH becomes an input
      o.nops(k);
      o.rd(0);
    }
    break;
  case 6: // what 240p Test Suite does at start
    o.wc(0, 0x40);
    o.wc(1, 0x7F);
    o.wd(0, 0x40);
    o.wd(1, 0x0C);
    o.rd(0);
    o.wd(1, 0x7C);
    o.rd(0);
    o.wc(0, 0x40);
    o.wc(1, 0x40);
    o.wc(2, 0x40);
    o.wd(0, 0x40);
    o.wd(1, 0x40);
    o.wd(2, 0x40);
    o.rd(0);
    o.rd(1);
    for (unsigned port : {0u, 1u})
      for (unsigned i = 0; i < 4; ++i) {
        o.wd(port, 0x00);
        o.rd(port);
        o.wd(port, 0x40);
        o.rd(port);
      }
    break;
  case 4: // reset of the counter by TH writes
    o.wc(0, 0x40);
    o.wd(0, 0x40);
    o.rd(0);
    o.wd(0, 0x00);
    o.rd(0);
    o.wd(0, 0x40);
    o.rd(0);
    o.wd(0, 0x00);
    o.rd(0);
    o.wc(0, 0x00);
    o.rd(0); // TH to input: pull up = rising?
    o.wd(0, 0x00);
    o.rd(0);
    o.wc(0, 0x40);
    o.wd(0, 0x00);
    o.rd(0); // output low again
    o.wd(0, 0x40);
    o.rd(0);
    o.wd(0, 0x00);
    o.rd(0);
    o.wd(0, 0x40);
    o.rd(0);
    o.wd(0, 0x00);
    o.rd(0);
    o.wd(0, 0x40);
    o.rd(0); // third rising edge: extra
    o.wc(0, 0x00);
    o.rd(0);
    o.wc(0, 0x40);
    o.rd(0);
    break;
  default:
    // Timeout: after a pause long enough to reset the pad, three TH cycles and
    // then a pause of d before the read of the low half of the third: the
    // read is the ID if the counter survived the pause. d grows by 100 loops.
    o.wc(0, 0x40);
    for (unsigned d = first; d <= last; d += step) {
      o.wd(0, 0x00);
      o.delay(3000);
      o.wd(0, 0x40);
      o.wd(0, 0x00);
      o.wd(0, 0x40);
      o.wd(0, 0x00);
      o.delay(d);
      o.rd(0);
    }
    break;
  }
  // clang-format off
  std::vector<std::uint16_t> c = {
      0x45F9,
      0x00FF,
      0x0000, // lea $FF0000,a2
  };
  // clang-format on
  c.insert(c.end(), o.c.begin(), o.c.end());
  c.push_back(0x60FE);
  std::vector<std::uint8_t> rom(0x200 + c.size() * 2 + 2, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER PAD LATCH", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom

namespace tessera::testrom {

// `raster_bands` (spec 008, TP-106.4, TP-203, TP-204, TP-205): a fixed scene of
// two planes and, every `reg10 + 1` lines, a horizontal interrupt whose
// handler writes the next entry of a table in ROM to the VDP. What it writes
// is the variant: a CRAM colour, the full screen vertical or horizontal
// scroll, or a register (backdrop, display enable, plane A base, H32/H40,
// shadow/highlight, the line counter itself). The vertical interrupt points
// the table back to its start and writes entry 0, so every frame shows the
// same bands; the bench compares the last frame and the VDP regions.
struct RasterVariant {
  std::uint8_t reg10 = 7;    // horizontal interrupt every reg10 + 1 lines
  std::uint32_t command = 0; // control longword before the data; 0: the
                             // entries are register writes to the control port
  unsigned words = 1;        // data words per entry
  std::uint8_t reg11 = 0x00; // full screen scroll
  std::uint8_t reg12 = 0x81; // H40
  std::vector<std::uint16_t> table; // 256 entries of `words` words
  // 68000 words run once after the scene is loaded, before the interrupts
  // (a0 = data port, a1 = control port).
  std::vector<std::uint16_t> post;
};

inline RasterVariant raster_variant(unsigned id) {
  RasterVariant v;
  const auto fill = [&](auto entry) {
    for (unsigned i = 0; i < 256; ++i)
      for (unsigned w = 0; w < v.words; ++w)
        v.table.push_back(static_cast<std::uint16_t>(entry(i, w)));
  };
  constexpr std::uint32_t kCram0 = 0xC0000000U;
  constexpr std::uint32_t kVsram0 = 0x40000010U;
  constexpr std::uint32_t kHscroll = 0x7C000003U; // VRAM $FC00
  const auto colour = [](unsigned i) {
    return (i * 0x0246U + 0x0420U) & 0x0EEEU;
  };
  switch (id) {
  case 0: // backdrop colour (CRAM 0) every 8 lines
    v.command = kCram0;
    fill([&](unsigned i, unsigned) { return colour(i); });
    break;
  case 1: // backdrop colour every line
    v.reg10 = 0;
    v.command = kCram0;
    fill([&](unsigned i, unsigned) { return colour(i); });
    break;
  case 2: // full screen vertical scroll of both planes, split screen
    v.command = kVsram0;
    v.words = 2;
    fill([](unsigned i, unsigned w) {
      return (i * (w == 0 ? 5U : 13U)) & 0x3FFU;
    });
    break;
  case 3: // full screen horizontal scroll of both planes
    v.command = kHscroll;
    v.words = 2;
    fill([](unsigned i, unsigned w) {
      return (w == 0 ? i * 7U : 0x400U - i * 4U) & 0x3FFU;
    });
    break;
  case 4: // backdrop index (register 7)
    fill([](unsigned i, unsigned) { return 0x8700U | ((i * 5U) & 0x3FU); });
    break;
  case 5: // display enable (register 1) on and off
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8124U : 0x8164U; });
    break;
  case 6: // plane A name table (register 2): $C000 and $8000
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8220U : 0x8230U; });
    break;
  case 7: // H40 and H32 (register 12)
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8C00U : 0x8C81U; });
    break;
  case 8: // shadow/highlight (register 12) off and on, H40
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8C89U : 0x8C81U; });
    break;
  case 9: // vertical scroll every line
    v.reg10 = 0;
    v.command = kVsram0;
    v.words = 2;
    fill([](unsigned i, unsigned w) {
      return (i * (w == 0 ? 3U : 1U)) & 0x3FFU;
    });
    break;
  case 10: // horizontal scroll every line
    v.reg10 = 0;
    v.command = kHscroll;
    v.words = 2;
    fill([](unsigned i, unsigned w) {
      return (i * (w == 0 ? 2U : 5U)) & 0x3FFU;
    });
    break;
  case 11: // the line counter (register 10) rewritten by its own interrupt
    v.reg10 = 3;
    fill([](unsigned i, unsigned) { return 0x8A00U | ((i % 4U) * 3U + 1U); });
    break;
  case 12: // PALETTE: shadow/highlight on and off after the CRAM is loaded,
           // then new colours in entries 16, 32 and 48
  case 13: // PALETTE: register 12 rewritten with the same value
  case 14: // PALETTE: limited palette (register 0) on and off
    fill([](unsigned, unsigned) { return 0x8700U; });
    if (id == 12)
      v.post = {0x32BC, 0x8C89, 0x32BC, 0x8C81};
    else if (id == 13)
      v.post = {0x32BC, 0x8C81};
    else
      v.post = {0x32BC, 0x8000, 0x32BC, 0x8004};
    for (const std::uint32_t entry : {16U, 32U, 48U})
      v.post.insert(v.post.end(),
                    {0x22BC, static_cast<std::uint16_t>(0xC000U | (entry * 2U)),
                     0x0000, 0x30BC,
                     static_cast<std::uint16_t>(0x0A0CU + entry)});
    break;
  // SYSTEM flags (GEOMETRY_PENDING): when does a size change leave it set?
  case 15: // control: register 12 rewritten with H40 at every band
    fill([](unsigned, unsigned) { return 0x8C81U; });
    break;
  case 16: // H32 in the vertical interrupt, H40 again at line 7
    fill([](unsigned i, unsigned) { return i == 0 ? 0x8C00U : 0x8C81U; });
    break;
  case 17: // H32 once at boot, nothing after
    fill([](unsigned, unsigned) { return 0x8700U; });
    v.post = {0x32BC, 0x8C00};
    break;
  case 18: // V30 in the vertical interrupt, V28 again at line 7
    fill([](unsigned i, unsigned) { return i == 0 ? 0x816CU : 0x8164U; });
    break;
  // RASTER_JOURNAL: which writes are events?
  case 19: // auto-increment (register 15)
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8F04U : 0x8F02U; });
    break;
  case 20: // DMA length (register 19)
    fill([](unsigned i, unsigned) { return 0x9300U | (i & 0xFFU); });
    break;
  case 21: // sprite table base (register 5): $F800 and $F000
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8578U : 0x857CU; });
    break;
  case 22: // VRAM outside every table ($A000)
    v.command = 0x60000002U;
    fill([](unsigned i, unsigned) { return i * 3U; });
    break;
  case 23: // VRAM in the plane A name table ($C000, first entry)
    v.command = 0x40000003U;
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x0005U : 0x0003U; });
    break;
  case 24: // CRAM entry 0 rewritten with the same colour
    v.command = kCram0;
    fill([](unsigned, unsigned) { return 0x0222U; });
    v.post = {0x22BC, 0xC000, 0x0000, 0x30BC, 0x0222};
    break;
  case 25: // VSRAM entries 2 and 3 (2-cell columns, unused in full mode)
    v.command = 0x40040010U;
    v.words = 2;
    fill([](unsigned i, unsigned w) { return (i * (w + 2U)) & 0x3FFU; });
    break;
  // LINE_REGS: when is each derived field taken?
  case 26: // plane B name table (register 4): $E000 and $8000
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8404U : 0x8407U; });
    break;
  case 27: // horizontal scroll table (register 13): $FC00 and $F400
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x8D3DU : 0x8D3FU; });
    break;
  case 28: // window name table (register 3): $F000 and $B000
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x832CU : 0x833CU; });
    break;
  case 29: // plane size (register 16): 64x32 and 32x32
    fill([](unsigned i, unsigned) { return (i & 1U) ? 0x9000U : 0x9001U; });
    break;
  case 30: // window columns (register 17): none, 10 cells left, 6 right
    fill([](unsigned i, unsigned) {
      return i % 3U == 0 ? 0x9100U : i % 3U == 1 ? 0x9105U : 0x9183U;
    });
    break;
  case 31: // window rows (register 18): none, top 12 rows, bottom from row 20
    fill([](unsigned i, unsigned) {
      return i % 3U == 0 ? 0x9200U : i % 3U == 1 ? 0x920CU : 0x9294U;
    });
    break;
  case 33: // CRAM entry 5: the journal address of a CRAM write
    v.command = 0xC00A0000U;
    fill(
        [](unsigned i, unsigned) { return (i * 0x0246U + 0x0420U) & 0x0EEEU; });
    break;
  case 32: // pattern data: row 7 of tiles 1 to 4 (VRAM $003C, $005C, ...)
    v.command = 0x403C0000U;
    v.words = 2;
    fill([](unsigned i, unsigned w) {
      return (i & 1U) ? (w ? 0x2345U : 0x6789U) : (w ? 0xABCDU : 0xEF12U);
    });
    break;
  default:
    fill([&](unsigned i, unsigned) { return colour(i); });
    v.command = kCram0;
    break;
  }
  return v;
}

/// Number of `raster_bands` variants; mirrored in
/// tools/diffbench/CMakeLists.txt.
inline unsigned raster_variant_count() { return 34; }

inline std::vector<std::uint8_t> raster_bands_rom(const RasterVariant &v) {
  std::vector<std::uint16_t> c = {
      0x41F9, 0x00C0, 0x0000, // lea $C00000,a0 (data port)
      0x43F9, 0x00C0, 0x0004, // lea $C00004,a1 (control port)
  };
  const auto reg = [&](std::uint8_t r, std::uint8_t value) {
    c.insert(c.end(),
             {0x32BC, static_cast<std::uint16_t>(0x8000 | (r << 8) | value)});
  };
  const auto command = [&](std::uint32_t cmd) {
    c.insert(c.end(), {0x22BC, static_cast<std::uint16_t>(cmd >> 16),
                       static_cast<std::uint16_t>(cmd)});
  };
  const auto vram = [&](std::uint16_t addr,
                        const std::vector<std::uint16_t> &words) {
    command((0x40000000U | ((addr & 0x3FFFU) << 16)) | ((addr >> 14) & 3U));
    for (const auto w : words)
      c.insert(c.end(), {0x30BC, w});
  };
  reg(0, 0x04); // full palette, no HINT while loading
  reg(1, 0x04); // display off, Mode 5
  reg(15, 2);
  reg(2, 0x30);  // plane A at $C000 (the second one at $8000)
  reg(4, 0x07);  // plane B at $E000
  reg(3, 0x3C);  // window at $F000, unused
  reg(5, 0x7C);  // sprites at $F800, empty
  reg(13, 0x3F); // horizontal scroll at $FC00
  reg(16, 0x01); // 64x32 cells
  reg(11, v.reg11);
  reg(12, v.reg12);
  reg(17, 0);
  reg(18, 0);
  reg(7, 0x00);
  reg(10, v.reg10);
  Lcg rng{0x5EED0B4DU};
  std::vector<std::uint16_t> tiles;
  for (unsigned t = 0; t < 32; ++t)
    for (unsigned row = 0; row < 8; ++row) {
      std::uint32_t word = 0;
      for (unsigned col = 0; col < 8; ++col) {
        const unsigned shade =
            (t == 0) ? 0U : (((t * 7U + row * 3U + col * 5U) % 15U) + 1U);
        word = (word << 4) | ((rng.next() % 4U == 0U) ? 0U : shade);
      }
      tiles.push_back(static_cast<std::uint16_t>(word >> 16));
      tiles.push_back(static_cast<std::uint16_t>(word));
    }
  vram(0x0000, tiles);
  const auto table = [&] {
    std::vector<std::uint16_t> t(64 * 32);
    for (auto &e : t) {
      const std::uint32_t r = rng.next();
      e = static_cast<std::uint16_t>(
          ((r & 0x100U) ? 0x8000U : 0U) | (((r >> 9) & 3U) << 13) |
          ((r & 0x800U) ? 0x1000U : 0U) | ((r & 0x400U) ? 0x0800U : 0U) |
          ((r >> 12) % 32U));
    }
    return t;
  };
  vram(0xC000, table());
  vram(0xE000, table());
  vram(0x8000, table());
  vram(0xF800, {0, 0, 0, 0}); // one sprite, off screen, end of the list
  vram(0xFC00, {0, 0});       // full screen horizontal scroll
  command(0xC0000000U);
  for (unsigned i = 0; i < 64; ++i)
    c.insert(c.end(),
             {0x30BC, static_cast<std::uint16_t>(rng.next() & 0x0EEEU)});
  command(0x40000010U);
  for (unsigned i = 0; i < 40; ++i)
    c.insert(c.end(), {0x30BC, std::uint16_t{0}});
  c.insert(c.end(), v.post.begin(), v.post.end());
  // lea table,a2 before the interrupts: the address is patched below.
  const std::size_t lea_at = c.size();
  c.insert(c.end(), {0x45F9, 0x0000, 0x0000});
  reg(0, 0x14);                      // HINT enabled
  reg(1, 0x64);                      // display, VINT, Mode 5
  c.insert(c.end(), {0x46FC, 0x2000, // move #$2000,sr
                     0x60FE});       // bra.s .
  // Handlers after the main code: HINT writes the next entry through a2,
  // VINT points a2 at the table and falls into it for entry 0.
  std::vector<std::uint16_t> hint;
  if (v.command != 0) {
    hint.insert(hint.end(),
                {0x23FC, static_cast<std::uint16_t>(v.command >> 16),
                 static_cast<std::uint16_t>(v.command), 0x00C0,
                 0x0004}); // move.l #cmd,$C00004
    for (unsigned w = 0; w < v.words; ++w)
      hint.insert(hint.end(), {0x33DA, 0x00C0, 0x0000}); // move.w (a2)+,$C00000
  } else {
    hint.insert(hint.end(), {0x33DA, 0x00C0, 0x0004}); // move.w (a2)+,$C00004
  }
  hint.push_back(0x4E73); // rte
  const std::uint32_t hint_at =
      0x200U + static_cast<std::uint32_t>(c.size()) * 2U;
  const std::uint32_t vint_at =
      hint_at + static_cast<std::uint32_t>(hint.size()) * 2U;
  // lea (6 bytes) and bra.w (4 bytes); the table follows. The bra.w sits at
  // vint_at + 6 and its displacement counts from vint_at + 8.
  const std::uint32_t table_at = vint_at + 10U;
  c[lea_at + 1] = static_cast<std::uint16_t>(table_at >> 16);
  c[lea_at + 2] = static_cast<std::uint16_t>(table_at);
  const auto back =
      static_cast<std::uint16_t>(static_cast<std::int32_t>(hint_at) -
                                 static_cast<std::int32_t>(vint_at + 8U));
  const std::uint16_t vint[] = {
      0x45F9,
      static_cast<std::uint16_t>(table_at >> 16),
      static_cast<std::uint16_t>(table_at), // lea table,a2
      0x6000,
      back, // bra.w hint
  };
  std::vector<std::uint8_t> rom(table_at + v.table.size() * 2 + 2, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  put32(rom, 0x70, hint_at); // level 4 autovector
  put32(rom, 0x78, vint_at); // level 6 autovector
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER RASTER   ", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : c) {
    put16(rom, at, word);
    at += 2;
  }
  for (const auto word : hint) {
    put16(rom, at, word);
    at += 2;
  }
  for (const auto word : vint) {
    put16(rom, at, word);
    at += 2;
  }
  for (const auto word : v.table) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}

} // namespace tessera::testrom
