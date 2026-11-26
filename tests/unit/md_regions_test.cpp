// TP-201.1 (RF-3.1): the memory regions. Reading gives the content of the
// machine in the layout of the contract and a poke is part of the state.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/regions.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <vector>

using namespace tessera::md;

namespace {
std::unique_ptr<System> booted(unsigned frames) {
  auto system =
      std::make_unique<System>(tessera::testrom::vint_counter_rom(),
                               MachineConfig::for_region(Region::usa));
  system->reset();
  for (unsigned i = 0; i < frames; ++i)
    system->run_frame();
  return system;
}
} // namespace

TEST_CASE("region sizes are the ones of the table") {
  CHECK(memory_region_size(kRegionVram) == 65536);
  CHECK(memory_region_size(kRegionCram) == 128);
  CHECK(memory_region_size(kRegionVdpRegs) == 32);
  CHECK(memory_region_size(kRegionVsram) == 128);
  CHECK(memory_region_size(kRegionZ80Ram) == 8192);
  CHECK(memory_region_size(kRegionWorkRam) == 65536);
  CHECK(memory_region_size(5) == 0); // LAYER_MASK is not a memory region
  const auto s = booted(1);
  CHECK(memory_region(*s, 5).empty());
  CHECK(memory_region(*s, kRegionVram).size() == 65536);
}

TEST_CASE("reading gives the content of the machine") {
  const auto s = booted(20);
  const auto vram = memory_region(*s, kRegionVram);
  for (std::size_t i = 0; i < vram.size(); ++i)
    REQUIRE(vram[i ^ 1U] == s->vdp().vram()[i]); // word-swapped

  const auto work = memory_region(*s, kRegionWorkRam);
  const auto ram = s->work_ram();
  for (std::size_t i = 0; i < work.size(); ++i)
    REQUIRE(work[i ^ 1U] == ram[i]);

  const auto z80 = memory_region(*s, kRegionZ80Ram);
  CHECK(std::equal(z80.begin(), z80.end(), s->z80_ram().begin()));

  const auto regs = memory_region(*s, kRegionVdpRegs);
  for (unsigned i = 0; i < 32; ++i)
    CHECK(regs[i] == s->vdp().reg(i));

  // colours: 0BBB0GGG0RRR in the VDP, BBBGGGRRR in the region
  s->write16(0xC00004, 0xC000); // CRAM write address 0
  s->write16(0xC00000, 0x0EC4); // B=7 G=6 R=2
  const auto cram = memory_region(*s, kRegionCram);
  CHECK((cram[0] | cram[1] << 8) == (7 << 6 | 6 << 3 | 2));
  CHECK(memory_region(*s, kRegionVsram).size() == 128);
}

TEST_CASE("reading changes nothing") {
  const auto a = booted(10);
  const auto b = booted(10);
  for (const auto id : {kRegionVram, kRegionCram, kRegionVdpRegs, kRegionVsram,
                        kRegionZ80Ram, kRegionWorkRam})
    (void)memory_region(*a, id);
  CHECK(a->save_state() == b->save_state());
}

TEST_CASE("a poke of the work RAM is part of the state") {
  const auto s = booted(10);
  const auto before = s->save_state();
  const std::vector<std::uint8_t> bytes = {0xDE, 0xAD, 0xBE, 0xEF};

  // region coordinates: the byte at region offset 0x1000 is the logical 0x1001
  REQUIRE(poke_memory_region(*s, kRegionWorkRam, 0x1000, bytes));
  CHECK(s->work_ram()[0x1001] == 0xDE);
  CHECK(s->work_ram()[0x1000] == 0xAD);
  CHECK(s->work_ram()[0x1003] == 0xBE);
  CHECK(s->work_ram()[0x1002] == 0xEF);
  const auto after = s->save_state();
  CHECK(after != before); // the savestate hash changes
  const auto read = memory_region(*s, kRegionWorkRam);
  CHECK(std::equal(bytes.begin(), bytes.end(), read.begin() + 0x1000));

  // it survives a round trip through the savestate
  const auto restored = booted(0);
  REQUIRE(restored->load_state(after));
  CHECK(restored->work_ram()[0x1001] == 0xDE);

  SUBCASE("poke at the logical offsets") {
    REQUIRE(s->poke_work_ram(0x20, bytes));
    CHECK(s->work_ram()[0x20] == 0xDE);
    CHECK(s->work_ram()[0x23] == 0xEF);
  }
  SUBCASE("limits and read-only regions") {
    CHECK_FALSE(poke_memory_region(*s, kRegionWorkRam, 65535, bytes));
    CHECK_FALSE(poke_memory_region(*s, kRegionWorkRam, 65536, bytes));
    CHECK_FALSE(poke_memory_region(*s, kRegionWorkRam, 0, {}));
    CHECK_FALSE(poke_memory_region(*s, kRegionVram, 0, bytes));
    CHECK_FALSE(poke_memory_region(*s, kRegionCram, 0, bytes));
    CHECK_FALSE(poke_memory_region(*s, kRegionVdpRegs, 0, bytes));
    CHECK_FALSE(poke_memory_region(*s, kRegionVsram, 0, bytes));
    CHECK(s->save_state() == after); // a refused poke changes nothing
    CHECK(poke_memory_region(*s, kRegionZ80Ram, 0x10, bytes));
    CHECK(s->z80_ram()[0x10] == 0xDE);
  }
}

TEST_CASE("SYSTEM describes the machine in the layout of the contract") {
  const auto rom = tessera::testrom::vint_counter_rom();
  auto s = booted(2);
  auto sys = memory_region(*s, kRegionSystem);
  REQUIRE(sys.size() == 44);
  const auto u16 = [&](std::size_t at) { return sys[at] | sys[at + 1] << 8; };
  CHECK(u16(0) == 44);
  CHECK(sys[8] == 0x80); // Mega Drive
  CHECK(sys[9] == 0);    // NTSC
  CHECK(sys[10] == 5);   // Mode 5
  CHECK(u16(14) == 262);
  CHECK(u16(20) == 256);
  CHECK(u16(22) == 224);
  CHECK(u16(24) == (7670453 & 0xFFFF));
  CHECK(u16(40) + (u16(42) << 16) == rom.size());

  CHECK(sys[35] == 0); // no GEOMETRY_PENDING

  // A size change describes the frame already drawn and raises
  // GEOMETRY_PENDING; the next frame takes the new size (gpgx, raster_bands).
  s->write16(0xC00004, 0x8C81); // register 12: H40
  s->write16(0xC00004, 0x817C); // register 1: V30 (bit 3)
  sys = memory_region(*s, kRegionSystem);
  CHECK(sys[12] == 0);
  CHECK(u16(20) == 256);
  CHECK(u16(22) == 224);
  CHECK(sys[35] == 1);
  s->run_frame();
  s->run_frame();
  sys = memory_region(*s, kRegionSystem);
  CHECK(sys[12] == 1);
  CHECK(u16(20) == 320);
  CHECK(u16(22) == 240);
  CHECK(sys[35] == 0);
}

TEST_CASE(
    "parsed sprites: only while captured, and as the reference sees them") {
  const auto rom =
      tessera::testrom::sprite_limits_rom(tessera::testrom::sprite_variant(3));
  const auto run = [&](bool capture) {
    auto s =
        std::make_unique<System>(rom, MachineConfig::for_region(Region::usa));
    s->reset();
    s->set_sprite_capture(capture);
    for (unsigned i = 0; i < 8; ++i)
      s->run_frame();
    return s;
  };
  const auto off = run(false);
  CHECK(memory_region(*off, kRegionParsedSpriteCount)[0] == 0);
  CHECK(memory_region(*off, kRegionParsedSprites).size() == 1280);

  const auto on = run(true);
  // sprite_limits 3 on gpgx r4: 39 of the 40 chained sprites reach a line
  // (the one at y = -11 never does), sorted by the first line they appear on
  CHECK(memory_region(*on, kRegionParsedSpriteCount)[0] == 39);
  const auto sprites = memory_region(*on, kRegionParsedSprites);
  CHECK(sprites[0] == 0x7A); // yr of the first one (sat 10)
  CHECK(sprites[8] == 10);   // sat_idx
  CHECK(sprites[9] == 10);   // chain_pos
  const auto outcome = memory_region(*on, kRegionSpriteOutcome);
  CHECK(outcome.size() == 80);
  CHECK(outcome[14] == 0); // y = -11: never on a line
  CHECK(outcome[24] == 1); // wholly left of the screen: listed, not drawn
  CHECK(outcome[0] == 3);  // listed and drawn
  CHECK(outcome[50] == 0); // not in the chain
}
