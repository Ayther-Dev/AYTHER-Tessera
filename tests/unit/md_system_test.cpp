// TP-101.9 (RF-2.2): the minimal machine boots a ROM, the VDP raises the
// vertical interrupt every frame and the 68000 takes it.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include "support/md_z80_testrom.hpp"
#include <doctest/doctest.h>

using namespace tessera::md;

namespace {
std::uint32_t counter(const System &system) {
  const auto ram = system.work_ram();
  return (std::uint32_t{ram[0]} << 24) | (std::uint32_t{ram[1]} << 16) |
         (std::uint32_t{ram[2]} << 8) | ram[3];
}
} // namespace

TEST_CASE("one vertical interrupt per frame reaches the handler") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::usa));
  system.reset();
  CHECK(counter(system) == 0);
  // gpgx raises no vertical interrupt on the first frame after a reset
  system.run_frame();
  CHECK(counter(system) == 0);
  system.run_frame();
  CHECK(counter(system) == 1);
  for (int i = 2; i < 100; ++i)
    system.run_frame();
  CHECK(counter(system) == 99);
  CHECK(system.frame() == 100);
}

TEST_CASE("PAL frames are longer but still one interrupt each") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::europe));
  system.reset();
  for (int i = 0; i < 25; ++i)
    system.run_frame();
  CHECK(counter(system) == 24); // none on the first frame
}

TEST_CASE("work RAM is cleared by a hard reset only through reset()") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::usa));
  system.reset();
  system.run_frame();
  system.run_frame();
  CHECK(counter(system) == 1);
  system.reset();
  CHECK(counter(system) == 0);
  CHECK(system.frame() == 0);
}

TEST_CASE("the I/O version register reports region and standard") {
  System usa(tessera::testrom::vint_counter_rom(),
             MachineConfig::for_region(Region::usa));
  CHECK(usa.io_version() == 0xA0);
  System eu(tessera::testrom::vint_counter_rom(),
            MachineConfig::for_region(Region::europe));
  CHECK(eu.io_version() == 0xE0);
  System jp(tessera::testrom::vint_counter_rom(),
            MachineConfig::for_region(Region::japan));
  CHECK(jp.io_version() == 0x20);
}

TEST_CASE("the Z80 bus request is granted at once") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::usa));
  system.reset();
  CHECK_FALSE(system.z80_bus_granted());
  system.write16(0xA11100, 0x0100);
  CHECK_FALSE(system.z80_bus_granted()); // the Z80 is still in its reset
  system.write16(0xA11200, 0x0100);
  CHECK(system.z80_bus_granted());
  CHECK((system.read16(0xA11100) & 0x0100) == 0);
  system.write16(0xA11100, 0x0000);
  CHECK_FALSE(system.z80_bus_granted());
  CHECK((system.read16(0xA11100) & 0x0100) != 0);
}

TEST_CASE("pad data port: three button protocol with the TH line") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::usa));
  system.reset();
  system.write8(0xA10009, 0x40); // port 1: TH is an output
  system.set_pad(0, 0x00);       // nothing pressed
  system.write8(0xA10003, 0x40); // TH = 1
  CHECK((system.read8(0xA10003) & 0x3F) == 0x3F);
  system.set_pad(0, kPadUp | kPadA | kPadC | kPadStart);
  system.write8(0xA10003, 0x40);
  CHECK((system.read8(0xA10003) & 0x3F) == (0x3F & ~0x01 & ~0x20)); // up, C
  system.write8(0xA10003, 0x00);                                    // TH = 0
  CHECK((system.read8(0xA10003) & 0x3F) ==
        (0x33 & ~0x01 & ~0x10 & ~0x20)); // up, A, start
}

// TP-103.3 (RF-2.1): requests and resets of the Z80, seen by the 68000.
TEST_CASE("the Z80 runs only out of reset and without a bus request") {
  System system(tessera::testrom::z80_bank_rom(0),
                MachineConfig::for_region(Region::usa));
  system.reset();
  for (int i = 0; i < 4; ++i)
    system.run_frame();
  // the program uploaded by the 68000 ran: end flag and one start
  const auto ram = system.z80_ram();
  CHECK(ram[0x1FF0] == 0xD0);
  CHECK(ram[0x1FF1] == 1);
  // the window write landed in work RAM, the mirrored byte was read back
  CHECK(system.work_ram()[0x7FFF] == 0x33);
  CHECK(ram[0x1000] == 0x5A);
}

TEST_CASE("a bus request is granted only with the Z80 out of reset") {
  System system(tessera::testrom::z80_bank_rom(1),
                MachineConfig::for_region(Region::usa));
  system.reset();
  for (int i = 0; i < 3; ++i)
    system.run_frame();
  // sequence 1 polls the grant with the Z80 still in reset: the poll count the
  // ROM logs first is the exhausted one (dbeq leaves -1)
  const auto ram = system.work_ram();
  CHECK(((ram[0] << 8) | ram[1]) == 0xFFFF);
}

// TP-209.2: a Z80 access to the 68000 bus through the bank window holds the
// 68000. z80_steal counts 68000 loop passes over the 224 active lines of four
// frames; the expected counts are gpgx's (black box, 2026-11-10): 3611 with
// the Z80 in reset, 3212 with a ROM read every 44 T-states, 3420 every 92.
// One pass of tolerance: the count is cut by the start of the blanking.
TEST_CASE("a Z80 access to the 68000 bus takes 68000 time as on gpgx") {
  const struct {
    unsigned nops;
    unsigned gpgx;
  } cases[] = {{99, 3611}, {4, 3212}, {16, 3420}};
  for (const auto &c : cases) {
    CAPTURE(c.nops);
    System system(tessera::testrom::z80_steal_rom(c.nops),
                  MachineConfig::for_region(Region::usa));
    system.reset();
    for (int i = 0; i < 10; ++i)
      system.run_frame();
    const auto ram = system.work_ram();
    for (unsigned n = 0; n < 4; ++n) {
      CAPTURE(n);
      const unsigned count = (ram[2 + 2 * n] << 8) | ram[3 + 2 * n];
      CHECK(count + 1 >= c.gpgx);
      CHECK(count <= c.gpgx + 1);
    }
  }
}

// TP-106.3 (RF-2.1): the vertical interrupt reaches the Z80 as a pulse of one
// scanline, about two handler entries per frame, with or without IE0.
TEST_CASE("the Z80 INT pulse is one scanline per frame") {
  System system(tessera::testrom::z80_int_rom(0),
                MachineConfig::for_region(Region::usa));
  system.reset();
  for (int i = 0; i < 12; ++i)
    system.run_frame();
  const auto ram = system.z80_ram();
  const unsigned index = ram[0x1F02] | (ram[0x1F03] << 8);
  CHECK(index == 0x1000 + 2 * 20); // 20 entries of two bytes in 12 frames
  const auto entry = [&](unsigned n) {
    return ram[0x1000 + 2 * n] | (ram[0x1001 + 2 * n] << 8);
  };
  CHECK(entry(0) == entry(1)); // the handler is entered twice back to back
  CHECK(entry(2) - entry(0) > 3290);
  CHECK(entry(2) - entry(0) < 3310);
}

// TP-106.3: when the vertical and the horizontal interrupt are both pending the
// 68000 takes level 6 first, then level 4.
TEST_CASE("level 6 is taken before level 4 when both are pending") {
  using namespace tessera::testrom;
  std::vector<std::uint8_t> rom(0x1000, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  put32(rom, 0x70, 0x300); // level 4
  put32(rom, 0x78, 0x310); // level 6
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  put32(rom, 0x1A4, 0xFFF);
  const std::vector<std::uint16_t> main_code = {
      0x4DF9, 0x00FF, 0x0000, // lea $FF0000,a6 (log)
      0x43F9, 0x00C0, 0x0004, // lea $C00004,a1
      0x32BC, 0x8164,         // mode 5, display on, vertical interrupt on
      0x32BC, 0x8014,         // horizontal interrupt on
      0x32BC, 0x8A00,         // ... on every line
      0x46FC, 0x2000,         // move #$2000,sr
      0x60FE,                 // bra *
  };
  std::size_t at = 0x200;
  for (const auto w : main_code) {
    put16(rom, at, w);
    at += 2;
  }
  put16(rom, 0x300, 0x3CFC); // level 4: move.w #4,(a6)+ ; rte
  put16(rom, 0x302, 0x0004);
  put16(rom, 0x304, 0x4E73);
  put16(rom, 0x310, 0x3CFC); // level 6: move.w #6,(a6)+ ; rte
  put16(rom, 0x312, 0x0006);
  put16(rom, 0x314, 0x4E73);
  System system(std::move(rom), MachineConfig::for_region(Region::usa));
  system.reset();
  for (int i = 0; i < 4; ++i)
    system.run_frame();
  const auto ram = system.work_ram();
  // every vertical interrupt (6) is followed by the horizontal one (4) of the
  // same line, never the other way round
  unsigned sixes = 0;
  for (unsigned i = 0; i + 1 < 1200; i += 2) {
    const unsigned level = (ram[i] << 8) | ram[i + 1];
    if (level == 6) {
      ++sixes;
      const unsigned next = (ram[i + 2] << 8) | ram[i + 3];
      CHECK(next == 4);
    }
  }
  CHECK(sixes >= 2);
}
