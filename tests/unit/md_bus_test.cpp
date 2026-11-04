// TP-101.4 (RF-2.1): 68000 bus of the native Mega Drive core: ROM, work RAM,
// Z80 area, I/O, VDP, mirrors and unmapped reads, round trip by region.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/bus.hpp"
#include <doctest/doctest.h>

#include <vector>

using namespace tessera::md;

namespace {
struct Spy : BusDevices {
  struct Access {
    Target target;
    std::uint32_t offset;
    std::uint16_t value;
    bool write;
    bool word;
  };
  std::vector<Access> log;
  std::uint16_t read(Target t, std::uint32_t offset, bool word) override {
    log.push_back({t, offset, 0, false, word});
    return word ? 0xBEEF : 0xAB;
  }
  void write(Target t, std::uint32_t offset, std::uint16_t v,
             bool word) override {
    log.push_back({t, offset, v, true, word});
  }
};

std::vector<std::uint8_t> rom_image(std::size_t n) {
  std::vector<std::uint8_t> rom(n);
  for (std::size_t i = 0; i < n; ++i)
    rom[i] = static_cast<std::uint8_t>(i * 7 + 3);
  return rom;
}
} // namespace

TEST_CASE("address classification by region") {
  CHECK(classify(0x000000) == Target::rom);
  CHECK(classify(0x3FFFFF) == Target::rom);
  CHECK(classify(0x400000) == Target::unmapped);
  CHECK(classify(0xA00000) == Target::z80_area);
  CHECK(classify(0xA0FFFF) == Target::z80_area);
  CHECK(classify(0xA10000) == Target::io);
  CHECK(classify(0xA1001F) == Target::io);
  CHECK(classify(0xA11100) == Target::z80_control);
  CHECK(classify(0xA11200) == Target::z80_control);
  CHECK(classify(0xA12000) == Target::unmapped);
  CHECK(classify(0xC00000) == Target::vdp);
  CHECK(classify(0xDFFFFF) == Target::vdp);
  CHECK(classify(0xE00000) == Target::work_ram);
  CHECK(classify(0xFFFFFF) == Target::work_ram);
  CHECK(classify(0x1000000) == classify(0x000000)); // 24 bit bus
}

TEST_CASE("ROM reads are big endian") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  CHECK(bus.read8(0) == 3);
  CHECK(bus.read16(0) == ((3 << 8) | 10));
  CHECK(bus.read32(0) == 0x030A1118u);
  CHECK(bus.read8(0x1FF) == static_cast<std::uint8_t>(0x1FF * 7 + 3));
}

TEST_CASE("ROM is read only and past its end is unmapped") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  bus.write16(0x10, 0x1234);
  CHECK(bus.read16(0x10) == ((static_cast<std::uint8_t>(0x10 * 7 + 3) << 8) |
                             static_cast<std::uint8_t>(0x11 * 7 + 3)));
  CHECK(bus.read8(0x200) == kUnmappedByte);
  CHECK(bus.read16(0x3FFFFE) == kUnmappedWord);
}

TEST_CASE("work RAM round trip and 64 KiB mirroring") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  bus.write8(0xFF0001, 0x5A);
  CHECK(bus.read8(0xFF0001) == 0x5A);
  bus.write16(0xFF0100, 0xCAFE);
  CHECK(bus.read16(0xFF0100) == 0xCAFE);
  CHECK(bus.read8(0xFF0100) == 0xCA);
  bus.write32(0xFF0200, 0x01234567);
  CHECK(bus.read32(0xFF0200) == 0x01234567u);
  CHECK(bus.read16(0xFF0202) == 0x4567);
  // mirror every 64 KiB across E00000-FFFFFF
  CHECK(bus.read16(0xE10100) == 0xCAFE);
  bus.write8(0xE20300, 0x77);
  CHECK(bus.read8(0xFF0300) == 0x77);
  CHECK(bus.work_ram().size() == 0x10000);
  CHECK(bus.work_ram()[0x100] == 0xCA);
}

TEST_CASE("devices receive the offset inside their region") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  CHECK(bus.read8(0xA00010) == 0xAB);
  CHECK(spy.log.back().target == Target::z80_area);
  CHECK(spy.log.back().offset == 0x10);
  CHECK_FALSE(spy.log.back().word);
  CHECK(bus.read16(0xA10002) == 0xBEEF);
  CHECK(spy.log.back().target == Target::io);
  CHECK(spy.log.back().offset == 2);
  CHECK(spy.log.back().word);
  bus.write16(0xA11100, 0x0100);
  CHECK(spy.log.back().target == Target::z80_control);
  CHECK(spy.log.back().offset == 0x1100);
  CHECK(spy.log.back().value == 0x0100);
  bus.write8(0xC00004, 0x81);
  CHECK(spy.log.back().target == Target::vdp);
  CHECK(spy.log.back().offset == 4);
  // the VDP ports mirror inside C00000-DFFFFF
  (void)bus.read16(0xD00004);
  CHECK(spy.log.back().offset == 4);
}

TEST_CASE("long accesses are two word accesses, high word first") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  bus.write32(0xC00000, 0xAABBCCDDu);
  REQUIRE(spy.log.size() == 2);
  CHECK(spy.log[0].offset == 0);
  CHECK(spy.log[0].value == 0xAABB);
  CHECK(spy.log[1].offset == 2);
  CHECK(spy.log[1].value == 0xCCDD);
}

TEST_CASE("unmapped regions read open bus and ignore writes") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  bus.write16(0x500000, 0x1111);
  CHECK(bus.read16(0x500000) == kUnmappedWord);
  CHECK(bus.read8(0xA12000) == kUnmappedByte);
  CHECK(spy.log.empty());
}

// ---- TP-101.7: SSF2 mapper
// ---------------------------------------------------
#include "support/md_testrom.hpp"

TEST_CASE("SSF2: the mapper registers are classified") {
  CHECK(classify(0xA130F1) == Target::mapper);
  CHECK(classify(0xA130F3) == Target::mapper);
  CHECK(classify(0xA130FF) == Target::mapper);
  CHECK(classify(0xA13100) == Target::unmapped);
}

TEST_CASE("SSF2: power-on layout maps every bank to itself") {
  Spy spy;
  Bus bus(tessera::testrom::ssf2_sweep_rom(), spy, Mapper::ssf2);
  // 4 MiB window: banks 0..7
  for (std::uint32_t bank = 0; bank < 8; ++bank) {
    CHECK(bus.read8(bank * 0x80000 + 0x40000) == 0xB0 + bank);
    CHECK(bus.read8(bank * 0x80000 + 0x7FFFF) == 0xC0 + bank);
  }
}

TEST_CASE("SSF2: each of the 16 banks is readable through every slot") {
  Spy spy;
  Bus bus(tessera::testrom::ssf2_sweep_rom(), spy, Mapper::ssf2);
  for (std::uint32_t slot = 1; slot < 8; ++slot) {
    for (std::uint32_t bank = 0; bank < 16; ++bank) {
      bus.write8(0xA130F1 + 2 * slot, static_cast<std::uint8_t>(bank));
      const std::uint32_t at = slot * 0x80000;
      CHECK(bus.read8(at + 0x40000) == 0xB0 + bank);
      CHECK(bus.read8(at + 0x7FFFF) == 0xC0 + bank);
      CHECK(bus.read16(at + 0x2000) == (((0xD0 + bank) << 8) & 0xFF00));
    }
  }
}

TEST_CASE(
    "SSF2: slot 0 is fixed and a register change leaves other slots alone") {
  Spy spy;
  Bus bus(tessera::testrom::ssf2_sweep_rom(), spy, Mapper::ssf2);
  bus.write8(0xA130F3, 9);                      // slot 1 -> bank 9
  CHECK(bus.read8(0x40000) == 0xB0);            // slot 0 still bank 0
  CHECK(bus.read8(0x80000 + 0x40000) == 0xB9);  // slot 1 -> bank 9
  CHECK(bus.read8(0x100000 + 0x40000) == 0xB2); // slot 2 untouched
  bus.write8(0xA130F1, 0);                      // not a bank register
  CHECK(bus.read8(0x40000) == 0xB0);
}

TEST_CASE("SSF2: word writes use the low byte, banks wrap over the ROM size") {
  Spy spy;
  Bus bus(tessera::testrom::ssf2_sweep_rom(), spy, Mapper::ssf2);
  bus.write16(0xA130F4,
              0x0003); // word at the even address, low byte lands on F5
  CHECK(bus.read8(0x100000 + 0x40000) == 0xB3);
  bus.write8(0xA130F5, 16 + 4); // beyond 8 MiB: wraps modulo 16 banks
  CHECK(bus.read8(0x100000 + 0x40000) == 0xB4);
}

TEST_CASE("without the mapper the registers are ignored") {
  Spy spy;
  Bus bus(rom_image(0x200), spy);
  bus.write8(0xA130F3, 5);
  CHECK(bus.read8(0x10) == static_cast<std::uint8_t>(0x10 * 7 + 3));
  CHECK(spy.log.empty());
}

// ---- TP-101.10: cartridge SRAM on the bus
// --------------------------------------
#include "md/sram.hpp"

namespace {
SramInfo odd_sram() {
  SramInfo info;
  info.present = true;
  info.start = 0x200001;
  info.end = 0x203FFF;
  info.bus = SramBus::odd;
  info.size_bytes = 0x2000;
  return info;
}
} // namespace

TEST_CASE("SRAM: hidden until $A130F1 maps it, then byte accesses reach it") {
  Spy spy;
  Sram sram(odd_sram());
  Bus bus(rom_image(0x400), spy, Mapper::none, &sram);
  CHECK(bus.read8(0x200001) == kUnmappedByte); // ROM is 1 KiB: nothing there
  bus.write8(0x200001, 0x12);                  // ignored while unmapped
  CHECK(sram.read8(0x200001) == 0xFF);

  bus.write8(0xA130F1, 0x01);
  bus.write8(0x200001, 0x12);
  bus.write8(0x200003, 0x34);
  CHECK(bus.read8(0x200001) == 0x12);
  CHECK(sram.read8(0x200003) == 0x34);
  CHECK(bus.read8(0x200000) == 0xFF); // even byte of an odd-bus SRAM
  CHECK(bus.read16(0x200002) == 0xFF34);
  CHECK(sram.dirty());

  bus.write8(0xA130F1, 0x00); // unmapped again
  CHECK(bus.read8(0x200001) == kUnmappedByte);
}

TEST_CASE("SRAM: write protect bit keeps the contents") {
  Spy spy;
  Sram sram(odd_sram());
  Bus bus(rom_image(0x400), spy, Mapper::none, &sram);
  bus.write8(0xA130F1, 0x01);
  bus.write8(0x200001, 0x55);
  bus.write8(0xA130F1, 0x03); // mapped + write protected
  bus.write8(0x200001, 0x66);
  CHECK(bus.read8(0x200001) == 0x55);
}

TEST_CASE("SRAM: outside the declared range the ROM answers") {
  Spy spy;
  Sram sram(odd_sram());
  Bus bus(rom_image(0x400), spy, Mapper::none, &sram);
  bus.write8(0xA130F1, 0x01);
  CHECK(bus.read8(0x204001) == kUnmappedByte);
}
