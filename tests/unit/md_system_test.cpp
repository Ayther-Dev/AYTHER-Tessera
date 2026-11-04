// TP-101.9 (RF-2.2): the minimal machine boots a ROM, the VDP raises the
// vertical interrupt every frame and the 68000 takes it.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_testrom.hpp"
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
  system.run_frame();
  CHECK(counter(system) == 1);
  for (int i = 1; i < 100; ++i)
    system.run_frame();
  CHECK(counter(system) == 100);
  CHECK(system.frame() == 100);
}

TEST_CASE("PAL frames are longer but still one interrupt each") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::europe));
  system.reset();
  for (int i = 0; i < 25; ++i)
    system.run_frame();
  CHECK(counter(system) == 25);
}

TEST_CASE("work RAM is cleared by a hard reset only through reset()") {
  System system(tessera::testrom::vint_counter_rom(),
                MachineConfig::for_region(Region::usa));
  system.reset();
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
