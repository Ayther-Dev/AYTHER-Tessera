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
  // the line starts at H = $85 (gpgx hvc.h) and passes H = 0 on cycle 760
  CHECK((v.read_hv(0) & 0xFF) == 0x85);
  CHECK((v.read_hv(760) & 0xFF) == 0x00);
  CHECK((v.read_hv(800) & 0xFF) < (v.read_hv(3000) & 0xFF));
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
  reg(v, 1, 0x14); // mode 5, DMA enabled, display off (blanking: 166 slots)
  reg(v, 15, 1);
  v.set_time(1000);
  dma_regs(v, 0x20, 0, 0x80);
  dma_command(v, 0x0100, 1);
  v.write_data(0xAB00);
  CHECK((v.read_status() & 0x0002) != 0);
  // the end of the last slot, rounded up, less the blanking start latency
  // of a fill (Vdp::fill_end, measured on gpgx)
  const std::uint64_t duration = (0x20ULL * 3420 + 165) / 166 - 85;
  v.set_time(1000 + duration - 1);
  CHECK((v.read_status() & 0x0002) != 0);
  v.set_time(1000 + duration);
  CHECK((v.read_status() & 0x0002) == 0);
}

// TP-209.2 (Sonic 1 title load): a fill started while the display is drawn
// takes the display slots only until the blanking starts; gpgx then finishes
// it at the blanking rate. Before, the rate of the first line was used for the
// whole fill, about 228 lines for 4095 bytes in H40.
TEST_CASE("a VRAM fill crossing into the blanking speeds up there") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x54);  // mode 5, DMA enabled, display on
  reg(v, 12, 0x81); // H40: 18 slots per drawn line, 204 per blanked line
  reg(v, 15, 1);
  v.begin_line(220);
  v.set_line_start(0);
  v.set_time(0);
  dma_regs(v, 0x0FFF, 0, 0x80); // 4095 bytes
  dma_command(v, 0x0100, 1);
  v.write_data(0xAB00);
  // 4 drawn lines give at most 4 * 18 bytes; the rest goes at 204 per line.
  const std::uint64_t blanking = 4ULL * 3420;
  const std::uint64_t rest = (0xFFFULL - 4 * 18) * 3420 / 204;
  v.set_time(blanking + rest - 3420);
  CHECK((v.read_status() & 0x0002) != 0);
  v.set_time(blanking + rest + 3420);
  CHECK((v.read_status() & 0x0002) == 0);
}

TEST_CASE("a VRAM copy takes two slots per byte and uses swapped bytes") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  set_write(v, 0x0000, 1);
  v.write_data(0x0123);
  v.write_data(0x1234);
  v.set_time(1000);
  dma_regs(v, 0x40, 0, 0xC0); // long enough to outlast the start latency
  reg(v, 21, 0);              // copy source is a VRAM byte address
  reg(v, 22, 0);

  v.write_control(0x0200);
  v.write_control(0x00C0);
  // every byte lands at (dest ^ 1) from (source ^ 1)
  CHECK(v.vram()[0x201] == 0x23);
  CHECK(v.vram()[0x203] == 0x01);
  // two slots per byte, less the blanking start latency of a copy
  const std::uint64_t duration = (0x40ULL * 2 * 3420 + 165) / 166 - 170;
  CHECK((v.read_status() & 0x0002) != 0);
  v.set_time(1000 + duration - 1);
  CHECK((v.read_status() & 0x0002) != 0);
  v.set_time(1000 + duration);
  CHECK((v.read_status() & 0x0002) == 0);
}

TEST_CASE("a 68000 to VDP DMA reports the cycles it stalls the CPU") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  v.set_dma_reader([](std::uint32_t) -> std::uint16_t { return 0x1111; });
  dma_regs(v, 16, 0x400, 0x00);
  dma_command(v, 0x0000, 1);
  // A VRAM word takes two external slots (measured on gpgx, TP-104.4).
  CHECK(v.take_cpu_stall() == (2ULL * 16 * 3420 + 166) / 167);
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

// TP-106.1 (RF-2.2): the V counter steps per mode. Values checked against gpgx
// for NTSC (V28 jumps from $EA to $E5, V30 does not jump); the PAL rows follow
// the documented jumps and are checked by the bench when a PAL ROM is run.
TEST_CASE("the V counter jump depends on the standard and on V28/V30") {
  const auto v_of = [](Standard s, unsigned v30, std::uint32_t line) {
    Vdp v(s);
    reg(v, 1, 0x04 | (v30 ? 0x08 : 0x00));
    v.begin_line(line);
    return static_cast<unsigned>(v.read_hv(0) >> 8);
  };
  CHECK(v_of(Standard::ntsc, 0, 0xEA) == 0xEA);
  CHECK(v_of(Standard::ntsc, 0, 0xEB) == 0xE5);
  CHECK(v_of(Standard::ntsc, 1, 0xF0) == 0xF0); // no jump in V30 on NTSC
  CHECK(v_of(Standard::ntsc, 1, 0xFF) == 0xFF);
  CHECK(v_of(Standard::pal, 0, 0x102) == 0x02);
  CHECK(v_of(Standard::pal, 0, 0x103) == 0xCA);
  CHECK(v_of(Standard::pal, 1, 0x10A) == 0x0A);
  CHECK(v_of(Standard::pal, 1, 0x10B) == 0xD2);
}

// TP-106.1 (RF-2.2): the H counter steps through 171 (H32) or 211 (H40) values
// per line and skips a range, as seen by gpgx (bench: hv-h32-h / hv-h40-h).
TEST_CASE("the H counter values per line and its jump in H32 and H40") {
  const auto seen = [](std::uint8_t reg12) {
    Vdp v(Standard::ntsc);
    reg(v, 1, 0x04);
    reg(v, 12, reg12);
    std::vector<bool> set(256, false);
    for (std::uint32_t c = 0; c < 3420; ++c)
      set[v.read_hv(c) & 0xFF] = true;
    return set;
  };
  const auto h32 = seen(0x00);
  const auto h40 = seen(0x81);
  for (unsigned h = 0; h < 256; ++h) {
    CHECK(h32[h] == (h <= 0x93 || h >= 0xE9));
    CHECK(h40[h] == (h <= 0xB6 || h >= 0xE4));
  }
}

// TP-104.6 (RF-2.2): VRAM access through the data port while the VDP is in
// Mode 4 (register 1 bit 2 clear), after gpgx vdp_68k_data_w_m4/_r_m4 (checked
// with the mode4_pbc bench): the word goes to the even address, byte swapped
// when the address is odd, a read returns the word of the even address (the
// byte access of the 68000 picks its low byte), and the address steps by
// register 15 + 1. Every control word is a whole
// command (gpgx vdp_68k_ctrl_w: pending is only set in Mode 5).
namespace {
std::uint16_t read_byte_low(Vdp &v) { return v.read_data() & 0xFF; }
void m4_set_write(Vdp &v, std::uint16_t address) {
  v.write_control(static_cast<std::uint16_t>(0x4000 | address));
}
void m4_set_read(Vdp &v, std::uint16_t address) { v.write_control(address); }
} // namespace

TEST_CASE("Mode 4 writes: one step per access, high byte then low byte") {
  Vdp v(Standard::ntsc);
  m4_set_write(v, 0x0000);
  v.write_data(0x1111); // a byte write is the byte in both halves
  v.write_data(0x2222);
  v.write_data(0x3333);
  CHECK(v.vram()[0] == 0x22); // the second write replaced the whole word
  CHECK(v.vram()[1] == 0x22);
  CHECK(v.vram()[2] == 0x33);
  CHECK(v.vram()[3] == 0x33);
  m4_set_write(v, 0x0010);
  v.write_data(0x4455);
  v.write_data(0x6677);
  // the second write is at the odd address $11: its word is byte swapped and
  // lands on the even word $10, whose low byte is at the even address
  CHECK(v.vram()[0x10] == 0x66);
  CHECK(v.vram()[0x11] == 0x77);
  CHECK(v.vram()[0x12] == 0x00);
}

TEST_CASE("Mode 4 reads: even word, low byte, one step per read") {
  Vdp v(Standard::ntsc);
  m4_set_write(v, 0x0000);
  v.write_data(0x1111);
  v.write_data(0x2222);
  v.write_data(0x3333);
  m4_set_read(v, 0x0000);
  CHECK(read_byte_low(v) == 0x22);
  CHECK(read_byte_low(v) == 0x22);
  CHECK(read_byte_low(v) == 0x33);
  CHECK(read_byte_low(v) == 0x33);
  // a read at an odd address returns the word of the even one below
  m4_set_write(v, 0x0030);
  v.write_data(0xAAAA);
  v.write_data(0xBBBB);
  m4_set_read(v, 0x0031);
  CHECK(read_byte_low(v) == 0xBB);
  CHECK(read_byte_low(v) == 0x00);
  CHECK(read_byte_low(v) == 0x00);
  // a word read gives both bytes of the even word
  m4_set_write(v, 0x0020);
  v.write_data(0x8888);
  m4_set_read(v, 0x0020);
  CHECK(v.read_data() == 0x8888);
  CHECK(v.read_data() == 0x8888);
}

TEST_CASE("Mode 4 CRAM writes: word index, 9 bit packing (gpgx probe)") {
  Vdp v(Standard::ntsc);
  v.write_control(0xC001); // CRAM write at word index 1, no second word
  v.write_data(0x0E4A);
  v.write_data(0x0123);
  CHECK(v.cram()[1] == 0x0E24);
  CHECK(v.cram()[2] == 0x0086);
  v.write_control(0xC020); // the index wraps at 32
  v.write_data(0x0E4A);
  CHECK(v.cram()[0] == 0x0E24);
}

TEST_CASE("Mode 4: every control word is a whole command") {
  Vdp v(Standard::ntsc);
  v.write_control(0x4005);
  v.write_data(0x7777);
  CHECK(v.vram()[5] == 0x77);
  // the zero second word of a long write is a read command at address 0 and
  // the writes that follow land at address 0 (gpgx), not at 5
  v.write_control(0x4005);
  v.write_control(0x0000);
  v.write_data(0x9999);
  CHECK(v.vram()[0] == 0x99);
  CHECK(v.vram()[2] == 0x00);
}

// TP-104.3 / TP-104.4: a finished DMA leaves the length at zero and moves the
// source registers on, as gpgx does (Sonic 1 depends on it).
TEST_CASE("a DMA leaves the length at zero and advances the source") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  v.set_dma_reader([](std::uint32_t) -> std::uint16_t { return 0x1111; });
  dma_regs(v, 16, 0x400, 0x00);
  dma_command(v, 0x0000, 1);
  CHECK(v.reg(19) == 0);
  CHECK(v.reg(20) == 0);
  CHECK(v.reg(21) == ((0x400 >> 1) + 16 & 0xFF));
  CHECK(v.reg(22) == (((0x400 >> 1) + 16) >> 8 & 0xFF));
}

// TP-104.3: the FIFO holds four words; the fifth write of a burst stalls the
// 68000 until a slot of the display frees an entry.
TEST_CASE("the fifth data port write of a burst stalls the CPU") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x44);
  reg(v, 15, 2);
  reg(v, 12, 0x81);
  v.begin_line(10);
  v.set_line_start(0);
  set_write(v, 0x0000, 3); // CRAM
  v.set_time(1000);
  for (int i = 0; i < 4; ++i)
    v.write_data(0x0EEE);
  CHECK(v.take_cpu_stall() == 0);
  v.write_data(0x0EEE);
  CHECK(v.take_cpu_stall() > 0);
}

TEST_CASE(
    "FIFO status bits: empty, then full after a burst, then empty again") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x44); // mode 5, display on
  reg(v, 15, 2);
  reg(v, 12, 0x81);
  v.begin_line(10);
  v.set_line_start(0);
  set_write(v, 0x0000, 1); // VRAM: two slots per word
  v.set_time(1000);
  CHECK((v.read_status() & 0x0300) == 0x0200); // empty, not full
  v.write_data(0x1111);
  CHECK((v.read_status() & 0x0300) == 0x0000); // neither
  for (int i = 0; i < 4; ++i)
    v.write_data(0x2222);
  CHECK(v.take_cpu_stall() > 0); // the fifth write waited for a free entry
  CHECK((v.read_status() & 0x0300) == 0x0100); // full, not empty
  v.set_time(1000 + 2 * 3420); // two lines later every entry is written
  CHECK((v.read_status() & 0x0300) == 0x0200);
}

TEST_CASE("the FIFO never fills in the blanking") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04); // display off: blanking slots on every line
  reg(v, 15, 2);
  v.begin_line(10);
  v.set_line_start(0);
  set_write(v, 0x0000, 1);
  std::uint64_t t = 1000;
  for (int i = 0; i < 8; ++i, t += 56) { // one move.w d0,(a0) apart
    v.set_time(t);
    v.write_data(0x3333);
    CHECK((v.read_status() & 0x0100) == 0);
  }
  CHECK(v.take_cpu_stall() == 0);
}

TEST_CASE("the last line of the frame is not blanked and has display slots") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x44);
  reg(v, 15, 2);
  reg(v, 12, 0x81);
  v.begin_line(260, false);
  CHECK((v.read_status() & 0x0008) != 0); // still the vertical blank
  v.begin_line(261, false);               // V = $FF
  CHECK((v.read_status() & 0x0008) == 0);
  v.set_line_start(0);
  set_write(v, 0x0000, 1);
  v.set_time(1000);
  for (int i = 0; i < 4; ++i)
    v.write_data(0x4444);
  CHECK(v.take_cpu_stall() == 0);
  v.write_data(0x4444);
  CHECK(v.take_cpu_stall() > 0); // as in the active display
}

// TP-104.3: the bits a read takes from the FIFO (measured on gpgx with VDP
// FIFO Testing). The entry is the one the next data port write goes to.
TEST_CASE("CRAM and VSRAM reads take the undriven bits from the FIFO") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04);
  reg(v, 15, 2);
  set_write(v, 0x8000, 1);
  for (std::uint16_t w : {0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666})
    v.write_data(w);
  // six writes into four entries: the next one is the third (3333)
  set_read(v, 0x0000, 4); // VSRAM
  CHECK(v.read_data() == 0x3000);
  CHECK(v.read_data() == 0x3000); // a read does not move the FIFO
  set_read(v, 0x0000, 8);         // CRAM
  CHECK(v.read_data() == 0x3111);
  set_write(v, 0x0020, 3); // CRAM write: one more entry
  v.write_data(0x0EEE);
  set_read(v, 0x0020, 8);
  CHECK(v.read_data() == 0x4EEE); // 4444 & F111 | colour
  set_read(v, 0x0000, 4);
  CHECK(v.read_data() == 0x4000);
}

TEST_CASE("a write to a read target goes through the FIFO and writes nothing") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04);
  reg(v, 15, 2);
  set_write(v, 0x0000, 1);
  for (std::uint16_t w : {0x1111, 0x2222, 0x3333, 0x4444})
    v.write_data(w);
  set_read(v, 0x0004, 4); // VSRAM read code
  v.write_data(0xFFFF);
  CHECK(v.vsram()[2] == 0);
  set_read(v, 0x0000, 4);
  CHECK(v.read_data() == 0x2000); // the FFFF took the 1111 entry
}

TEST_CASE(
    "8-bit VRAM read: the byte of address ^ 1, the high byte from the FIFO") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04);
  reg(v, 15, 2);
  set_write(v, 0x8000, 1);
  for (std::uint16_t w : {0x1122, 0x3344, 0x5566, 0x7788, 0x99AA})
    v.write_data(w);
  set_read(v, 0x8000, 0x0C);
  CHECK(v.read_data() == 0x3322); // entry 3344
  CHECK(v.read_data() == 0x3344);
  reg(v, 15, 1);
  set_read(v, 0x8001, 0x0C);
  CHECK(v.read_data() == 0x3311);
  CHECK(v.read_data() == 0x3344);
}

TEST_CASE("68000 DMA words and the fill word enter the FIFO") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x14); // mode 5, DMA enabled, display off
  reg(v, 15, 2);
  std::uint16_t next = 0xAAAA;
  v.set_dma_reader([&next](std::uint32_t) -> std::uint16_t {
    const std::uint16_t w = next;
    next = static_cast<std::uint16_t>(next + 0x1111);
    return w;
  });
  dma_regs(v, 6, 0x1000, 0x00);
  dma_command(v, 0x8000, 1); // AAAA BBBB CCCC DDDD EEEE FFFF
  set_read(v, 0x0000, 4);
  CHECK(v.read_data() == 0xC800);
  reg(v, 15, 1);
  dma_regs(v, 4, 0, 0x80);
  dma_command(v, 0x9000, 1);
  v.write_data(0x1234); // fill word
  set_read(v, 0x0000, 4);
  CHECK(v.read_data() == 0xD800); // the fill took the CCCC entry
}
