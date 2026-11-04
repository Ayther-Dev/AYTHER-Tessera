// TP-101.9 (RF-2.2): minimal boot VDP. Control and data ports, register
// writes, VRAM/CRAM/VSRAM access with auto-increment, DMA from the 68000 bus,
// status flags, vertical interrupt and basic H/V counters.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/vdp.hpp"
#include <doctest/doctest.h>

#include <vector>

using namespace tessera::md;

namespace {
// Command words for a VRAM write to `addr` ($4000 | addr, then $0000).
void set_write(Vdp &v, std::uint16_t addr, std::uint8_t code = 1) {
  v.write_control(
      static_cast<std::uint16_t>(((code & 3) << 14) | (addr & 0x3FFF)));
  v.write_control(
      static_cast<std::uint16_t>(((addr >> 14) & 3) | ((code & 0x3C) << 2)));
}
void set_read(Vdp &v, std::uint16_t addr, std::uint8_t code) {
  set_write(v, addr, code);
}
void reg(Vdp &v, int r, int value) {
  v.write_control(static_cast<std::uint16_t>(0x8000 | (r << 8) | value));
}
} // namespace

TEST_CASE("register writes through the control port") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x64);
  reg(v, 15, 2);
  CHECK(v.reg(1) == 0x64);
  CHECK(v.reg(15) == 2);
}

TEST_CASE("VRAM write with auto-increment and read back") {
  Vdp v(Standard::ntsc);
  reg(v, 15, 2);
  set_write(v, 0x0100);
  v.write_data(0x1234);
  v.write_data(0x5678);
  CHECK(v.vram()[0x100] == 0x12);
  CHECK(v.vram()[0x101] == 0x34);
  CHECK(v.vram()[0x102] == 0x56);
  CHECK(v.vram()[0x103] == 0x78);
  set_read(v, 0x0100, 0x00);
  CHECK(v.read_data() == 0x1234);
  CHECK(v.read_data() == 0x5678);
}

TEST_CASE("CRAM and VSRAM writes") {
  Vdp v(Standard::ntsc);
  reg(v, 15, 2);
  set_write(v, 0x0004, 3); // CRAM write
  v.write_data(0x0EEE);
  CHECK(v.cram()[2] == 0x0EEE);
  set_write(v, 0x0002, 5); // VSRAM write
  v.write_data(0x0123);
  CHECK(v.vsram()[1] == 0x0123);
}

TEST_CASE("a control word read of the status clears a half-written command") {
  Vdp v(Standard::ntsc);
  v.write_control(0x4000); // first half
  (void)v.read_status();
  reg(v, 15, 1); // must be taken as a register write, not as a second half
  CHECK(v.reg(15) == 1);
}

TEST_CASE("fill DMA") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x10); // DMA enabled
  reg(v, 15, 1);
  reg(v, 19, 4); // length low
  reg(v, 20, 0);
  reg(v, 23, 0x80);           // fill
  set_write(v, 0x0200, 0x21); // VRAM write with DMA bit
  v.write_data(0xABAB);       // data starts the fill
  for (unsigned i = 0; i < 4; ++i)
    CHECK(v.vram()[0x200 + i] == 0xAB);
}

TEST_CASE("68000 to VRAM DMA reads words from the bus") {
  Vdp v(Standard::ntsc);
  v.set_dma_reader([](std::uint32_t address) -> std::uint16_t {
    return static_cast<std::uint16_t>(0x1000 + (address >> 1));
  });
  reg(v, 1, 0x10);
  reg(v, 15, 2);
  reg(v, 19, 2); // 2 words
  reg(v, 20, 0);
  reg(v, 21, 0x00); // source = 0x000000 (address/2 in regs 21-23)
  reg(v, 22, 0x00);
  reg(v, 23, 0x00);
  set_write(v, 0x0300, 0x21);
  CHECK(v.vram()[0x300] == 0x10);
  CHECK(v.vram()[0x301] == 0x00);
  CHECK(v.vram()[0x302] == 0x10);
  CHECK(v.vram()[0x303] == 0x01);
}

TEST_CASE("vertical interrupt at the first line after the active display") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x60); // display on, VINT enabled
  for (unsigned line = 0; line < 224; ++line)
    v.begin_line(line);
  CHECK(v.irq_level() == 0);
  CHECK((v.read_status() & 0x80) == 0);
  v.begin_line(224);
  CHECK(v.irq_level() == 6);
  CHECK((v.read_status() & 0x80) != 0);
  v.acknowledge(6);
  CHECK(v.irq_level() == 0);
}

TEST_CASE("VINT enable bit gates the interrupt, not the status flag") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x40); // VINT disabled
  v.begin_line(224);
  CHECK(v.irq_level() == 0);
  CHECK((v.read_status() & 0x80) != 0);
}

TEST_CASE("vblank flag and PAL bit in the status") {
  Vdp ntsc(Standard::ntsc);
  ntsc.begin_line(10);
  CHECK((ntsc.read_status() & 0x08) == 0);
  ntsc.begin_line(230);
  CHECK((ntsc.read_status() & 0x08) != 0);
  CHECK((ntsc.read_status() & 0x01) == 0);
  Vdp pal(Standard::pal);
  CHECK((pal.read_status() & 0x01) != 0);
}

TEST_CASE("V counter follows the line, H counter the position in it") {
  Vdp v(Standard::ntsc);
  v.begin_line(100);
  CHECK((v.read_hv(0) >> 8) == 100);
  v.begin_line(234);
  CHECK((v.read_hv(0) >> 8) == 0xEA);
  v.begin_line(235); // NTSC counter jumps back
  CHECK((v.read_hv(0) >> 8) == 0xE5);
  v.begin_line(261);
  CHECK((v.read_hv(0) >> 8) == 0xFF);
  CHECK((v.read_hv(0) & 0xFF) < (v.read_hv(3000) & 0xFF));
}

TEST_CASE("horizontal interrupt counts lines") {
  Vdp v(Standard::ntsc);
  reg(v, 0, 0x14); // HINT enabled
  reg(v, 10, 2);   // every 3 lines
  v.begin_line(0);
  v.begin_line(1);
  CHECK(v.irq_level() == 0);
  v.begin_line(2);
  CHECK(v.irq_level() == 4);
  v.acknowledge(4);
  CHECK(v.irq_level() == 0);
}
