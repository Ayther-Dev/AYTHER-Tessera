// TP-101.5 (RF-2.5): optional TMSS. With TMSS a ROM without "SEGA" in its
// header does not boot; without TMSS it does. The enable register is at
// $A14000 and takes the ASCII word "SEGA".
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/bus.hpp"
#include "md/machine.hpp"
#include "md/tmss.hpp"
#include <doctest/doctest.h>

#include <cstring>
#include <vector>

using namespace tessera::md;

namespace {
std::vector<std::uint8_t> rom_with(const char *console,
                                   std::size_t at = 0x100) {
  std::vector<std::uint8_t> rom(0x400, 0);
  std::memcpy(&rom[at], console, std::strlen(console));
  return rom;
}
} // namespace

TEST_CASE("with TMSS a ROM without SEGA does not boot, without TMSS it does") {
  MachineConfig with_tmss;
  with_tmss.tmss = true;
  MachineConfig without_tmss;
  without_tmss.tmss = false;

  const auto sega = rom_with("SEGA MEGA DRIVE");
  const auto shifted = rom_with(" SEGA", 0x100); // " SEGA": SEGA at $101
  const auto none = rom_with("HOMEBREW");

  CHECK(can_boot(with_tmss, sega));
  CHECK(can_boot(with_tmss, shifted));
  CHECK_FALSE(can_boot(with_tmss, none));
  CHECK(can_boot(without_tmss, none));
  CHECK(can_boot(without_tmss, sega));
  CHECK_FALSE(
      can_boot(with_tmss, std::vector<std::uint8_t>(0x100, 0))); // too short
}

TEST_CASE("the register unlocks only with the word SEGA") {
  Tmss tmss(true);
  CHECK(tmss.locked());
  tmss.write(0, 0x5345, true);
  CHECK(tmss.locked()); // half is not enough
  tmss.write(2, 0x0000, true);
  CHECK(tmss.locked());
  tmss.write(0, 0x5345, true);
  tmss.write(2, 0x4741, true);
  CHECK_FALSE(tmss.locked());
  tmss.reset();
  CHECK(tmss.locked());
}

TEST_CASE("the register also accepts four byte writes") {
  Tmss tmss(true);
  const char sega[] = {'S', 'E', 'G', 'A'};
  for (std::uint32_t i = 0; i < 4; ++i)
    tmss.write(i, static_cast<std::uint8_t>(sega[i]), false);
  CHECK_FALSE(tmss.locked());
}

TEST_CASE("without TMSS the console is never locked") {
  Tmss tmss(false);
  CHECK_FALSE(tmss.locked());
  tmss.reset();
  CHECK_FALSE(tmss.locked());
}

TEST_CASE("the register is mapped on the bus") {
  CHECK(classify(0xA14000) == Target::tmss);
  CHECK(classify(0xA14003) == Target::tmss);
  CHECK(classify(0xA14004) == Target::unmapped);
  CHECK(classify(0xA13FFF) == Target::unmapped);
}
