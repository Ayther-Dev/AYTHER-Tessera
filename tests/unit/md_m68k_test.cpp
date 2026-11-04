// TP-101.8 (RF-2.1): Moira behind the native Mega Drive bus. A synthetic
// program runs on the 68000 and the final work RAM is the expected one;
// reset, exceptions and the interrupt priority level are exercised too.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/bus.hpp"
#include "md/m68k.hpp"
#include <doctest/doctest.h>

#include <initializer_list>
#include <vector>

using namespace tessera::md;

namespace {
struct NoDevices : BusDevices {
  std::uint16_t read(Target, std::uint32_t, bool) override { return 0xFFFF; }
  void write(Target, std::uint32_t, std::uint16_t, bool) override {}
};

struct Image {
  std::vector<std::uint8_t> rom = std::vector<std::uint8_t>(0x1000, 0);
  void w16(std::size_t at, std::uint16_t v) {
    rom[at] = static_cast<std::uint8_t>(v >> 8);
    rom[at + 1] = static_cast<std::uint8_t>(v);
  }
  void w32(std::size_t at, std::uint32_t v) {
    w16(at, static_cast<std::uint16_t>(v >> 16));
    w16(at + 2, static_cast<std::uint16_t>(v));
  }
  void code(std::size_t at, std::initializer_list<std::uint16_t> words) {
    for (const auto word : words) {
      w16(at, word);
      at += 2;
    }
  }
  Image() {
    w32(0, 0xFFFE00); // initial SP
    w32(4, 0x200);    // initial PC
  }
};
} // namespace

TEST_CASE("reset loads SP and PC from the vector table") {
  NoDevices devices;
  Image image;
  image.code(0x200, {0x4E71}); // nop
  Bus bus(image.rom, devices);
  M68k cpu(bus);
  cpu.reset();
  CHECK(cpu.getPC() == 0x200);
  CHECK(cpu.getA(7) == 0xFFFE00);
}

TEST_CASE("a synthetic program leaves the expected work RAM") {
  NoDevices devices;
  Image image;
  image.code(0x200, {
                        0x23FC,
                        0x1234,
                        0x5678,
                        0x00FF,
                        0x0000, // move.l #$12345678,$FF0000
                        0x33FC,
                        0xABCD,
                        0x00FF,
                        0x0004, // move.w #$ABCD,$FF0004
                        0x41F9,
                        0x00FF,
                        0x0010, // lea    $FF0010,a0
                        0x7009, // moveq  #9,d0
                        0x10C0, // loop: move.b d0,(a0)+
                        0x51C8,
                        0xFFFC, //       dbra d0,loop
                        0x4E72,
                        0x2700, // stop   #$2700
                    });
  Bus bus(image.rom, devices);
  M68k cpu(bus);
  cpu.reset();
  cpu.execute(2000);
  const auto ram = bus.work_ram();
  CHECK(ram[0] == 0x12);
  CHECK(ram[1] == 0x34);
  CHECK(ram[2] == 0x56);
  CHECK(ram[3] == 0x78);
  CHECK(ram[4] == 0xAB);
  CHECK(ram[5] == 0xCD);
  for (unsigned i = 0; i < 10; ++i)
    CHECK(ram[0x10 + i] == 9 - i);
  CHECK(cpu.getA(0) == 0xFF001A);
  CHECK((cpu.getD(0) & 0xFFFF) == 0xFFFF);
  CHECK(cpu.getClock() > 0);
}

TEST_CASE("an illegal instruction takes the illegal-instruction vector") {
  NoDevices devices;
  Image image;
  image.w32(4 * 4, 0x400);             // vector 4
  image.code(0x200, {0x4AFC, 0x60FE}); // illegal ; bra.s .
  image.code(0x400, {0x13FC, 0x0055, 0x00FF, 0x0020,
                     0x60FE}); // move.b #$55,$FF0020 ; bra.s .
  Bus bus(image.rom, devices);
  M68k cpu(bus);
  cpu.reset();
  cpu.execute(500);
  CHECK(bus.work_ram()[0x20] == 0x55);
}

TEST_CASE("the interrupt priority level is taken when the mask allows it") {
  NoDevices devices;
  Image image;
  image.w32(0x70, 0x300);            // level 4 autovector (vector 28)
  image.code(0x200, {0x46FC, 0x2000, // move #$2000,sr   (mask 0)
                     0x60FE});       // bra.s .
  image.code(0x300, {0x13FC, 0x0077, 0x00FF, 0x0030, // move.b #$77,$FF0030
                     0x4E73});                       // rte
  Bus bus(image.rom, devices);
  M68k cpu(bus);
  cpu.reset();
  cpu.execute(200);
  CHECK(bus.work_ram()[0x30] == 0);
  cpu.setIPL(4);
  cpu.execute(200);
  CHECK(bus.work_ram()[0x30] == 0x77);
}

TEST_CASE("an interrupt masked by SR is not taken") {
  NoDevices devices;
  Image image;
  image.w32(0x70, 0x300);
  image.code(0x200, {0x60FE}); // stays at mask 7 after reset
  image.code(0x300, {0x13FC, 0x0077, 0x00FF, 0x0030, 0x4E73});
  Bus bus(image.rom, devices);
  M68k cpu(bus);
  cpu.reset();
  cpu.setIPL(4);
  cpu.execute(200);
  CHECK(bus.work_ram()[0x30] == 0);
}
