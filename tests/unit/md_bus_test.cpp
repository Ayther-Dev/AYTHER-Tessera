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
