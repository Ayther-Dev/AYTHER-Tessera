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
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
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
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
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
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
  reg(v, 15, 1);   // must be taken as a register write, not as a second half
  CHECK(v.reg(15) == 1);
}

TEST_CASE("fill DMA") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14); // DMA enabled
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
  reg(v, 1, 0x14);
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
  reg(ntsc, 1, 0x40); // display on
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

// ---- TP-104.1: ports
// ------------------------------------------------------------

TEST_CASE("CRAM and VSRAM read back through the data port") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
  reg(v, 15, 2);
  set_write(v, 0x0010, 3);
  v.write_data(0x0E0E);
  v.write_data(0x00AA);
  set_read(v, 0x0010, 8);
  CHECK(v.read_data() == 0x0E0E);
  CHECK(v.read_data() == 0x00AA);
  set_write(v, 0x0004, 5);
  v.write_data(0x03FF);
  set_read(v, 0x0004, 4);
  CHECK(v.read_data() == 0x03FF);
}

TEST_CASE("the auto-increment register steps by 1, 2 and 4") {
  for (const int step : {1, 2, 4}) {
    Vdp v(Standard::ntsc);
    reg(v, 1, 0x04); // mode 5: registers above 10 exist
    reg(v, 15, step);
    set_write(v, 0x0100);
    v.write_data(0xAAAA);
    v.write_data(0xBBBB);
    // the second word lands `step` bytes after the first one
    const unsigned second = 0x100 + step;
    if (step == 2 || step == 4) {
      CHECK(v.vram()[second] == 0xBB);
      CHECK(v.vram()[second + 1] == 0xBB);
    }
    if (step == 1) { // odd address: the bytes are swapped inside the word
      CHECK(v.vram()[0x101] == 0xBB);
      CHECK(v.vram()[0x100] == 0xBB);
    }
  }
}

TEST_CASE("the access address uses 16 bits: second word carries bits 14-15") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
  reg(v, 15, 2);
  set_write(v, 0x8000);
  v.write_data(0x1122);
  CHECK(v.vram()[0x8000] == 0x11);
  CHECK(v.vram()[0x8001] == 0x22);
  set_write(v, 0xFFFE);
  v.write_data(0x3344);
  v.write_data(0x5566); // wraps to address 0
  CHECK(v.vram()[0xFFFE] == 0x33);
  CHECK(v.vram()[0xFFFF] == 0x44);
  CHECK(v.vram()[0x0000] == 0x55);
  CHECK(v.vram()[0x0001] == 0x66);
}

TEST_CASE("an odd VRAM address stores the word with its bytes swapped") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
  reg(v, 15, 2);
  set_write(v, 0x0201);
  v.write_data(0xA1B2);
  CHECK(v.vram()[0x200] == 0xB2);
  CHECK(v.vram()[0x201] == 0xA1);
}

TEST_CASE("CRAM keeps 9 bit colours and VSRAM 11 bit scroll values") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
  reg(v, 15, 2);
  set_write(v, 0x0000, 3);
  v.write_data(0xFFFF);
  CHECK(v.cram()[0] == 0x0EEE);
  set_write(v, 0x0000, 5);
  v.write_data(0xFFFF);
  CHECK(v.vsram()[0] == 0x07FF);
}

TEST_CASE("a register write does not disturb the pending access address") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04); // mode 5: registers above 10 exist
  reg(v, 15, 2);
  set_write(v, 0x0040);
  reg(v, 0, 0x04); // set a register between the command and the data
  v.write_data(0x7777);
  CHECK(v.reg(0) == 0x04);
  // the register write resets the access code, so the data goes nowhere
  CHECK(v.vram()[0x40] == 0x00);
}

TEST_CASE("the status register is stable across reads and has the fixed bits") {
  Vdp v(Standard::ntsc);
  const auto first = v.read_status();
  CHECK(first == v.read_status());
  CHECK((first & 0xFC00) == 0x3400);
  CHECK((first & 0x0200) != 0); // FIFO empty
}

// TP-104.4 (RF-2.2): DMA duration, busy bit and blank bit.
namespace {
void dma_regs(Vdp &v, std::uint16_t length, std::uint32_t source,
              std::uint8_t mode_bits) {
  reg(v, 19, length & 0xFF);
  reg(v, 20, length >> 8);
  reg(v, 21, (source >> 1) & 0xFF);
  reg(v, 22, (source >> 9) & 0xFF);
  reg(v, 23, static_cast<int>(((source >> 17) & 0x3F) | mode_bits));
}
void dma_command(Vdp &v, std::uint16_t addr, std::uint8_t code) {
  v.write_control(
      static_cast<std::uint16_t>(((code & 3) << 14) | (addr & 0x3FFF)));
  v.write_control(static_cast<std::uint16_t>(0x0080 | ((code & 0x3C) << 2)));
}
} // namespace

TEST_CASE("the blank bit follows the display enable and the active area") {
  Vdp v(Standard::ntsc);
  CHECK((v.read_status() & 0x0008) != 0); // display off after reset
  reg(v, 1, 0x74);
  v.begin_line(10);
  CHECK((v.read_status() & 0x0008) == 0);
  v.begin_line(224);
  CHECK((v.read_status() & 0x0008) != 0);
}

TEST_CASE("registers above 10 are ignored while the VDP is in mode 4") {
  Vdp v(Standard::ntsc);
  reg(v, 15, 2); // register 1 bit 2 still clear
  CHECK(v.reg(15) == 0);
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  CHECK(v.reg(15) == 2);
}

TEST_CASE("a VRAM fill keeps the busy bit set for one slot per byte") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14); // mode 5, DMA enabled, display off (blanking: 167 slots)
  reg(v, 15, 1);
  v.set_time(1000);
  dma_regs(v, 0x20, 0, 0x80);
  dma_command(v, 0x0100, 1);
  v.write_data(0xAB00);
  CHECK((v.read_status() & 0x0002) != 0);
  const std::uint64_t duration = 0x20ULL * 3420 / 167;
  v.set_time(1000 + duration - 1);
  CHECK((v.read_status() & 0x0002) != 0);
  v.set_time(1000 + duration);
  CHECK((v.read_status() & 0x0002) == 0);
}

TEST_CASE("a VRAM copy takes two slots per byte and uses swapped bytes") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  set_write(v, 0x0000, 1);
  v.write_data(0x0123);
  v.write_data(0x1234);
  v.set_time(0);
  dma_regs(v, 4, 0, 0xC0);
  reg(v, 21, 0); // copy source is a VRAM byte address
  reg(v, 22, 0);

  v.write_control(0x0200);
  v.write_control(0x00C0);
  // every byte lands at (dest ^ 1) from (source ^ 1)
  CHECK(v.vram()[0x201] == 0x23);
  CHECK(v.vram()[0x203] == 0x01);
  const std::uint64_t duration = 4ULL * 2 * 3420 / 167;
  CHECK((v.read_status() & 0x0002) != 0);
  v.set_time(duration);
  CHECK((v.read_status() & 0x0002) == 0);
}

TEST_CASE("a 68000 to VDP DMA reports the cycles it stalls the CPU") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  v.set_dma_reader([](std::uint32_t) -> std::uint16_t { return 0x1111; });
  dma_regs(v, 16, 0x400, 0x00);
  dma_command(v, 0x0000, 1);
  CHECK(v.take_cpu_stall() == 16ULL * 3420 / 167);
  CHECK(v.take_cpu_stall() == 0);
}

// TP-106.2 (RF-2.2): gpgx raises the horizontal interrupt on lines 0..224
// (the line after the active display included) with register 10 = 0, and
// only the vertical blank counter reload stops it after that.
TEST_CASE("the horizontal interrupt also fires on the line after the display") {
  Vdp v(Standard::ntsc);
  reg(v, 0, 0x14);
  reg(v, 10, 0);
  unsigned fired = 0;
  for (std::uint32_t line = 0; line < 262; ++line) {
    v.begin_line(line);
    if (v.irq_level() == 4) {
      ++fired;
      v.acknowledge(4);
    }
  }
  CHECK(fired == 225);
}
