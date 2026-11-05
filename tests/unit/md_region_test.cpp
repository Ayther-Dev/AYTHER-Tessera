// TP-108.4 (RF-2.4, PQ-13): the console region comes from the ROM header, with
// manual selection on top. The table is the observable behaviour of gpgx,
// which integration.md_region-detect-* checks on the real machines.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/region.hpp"
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <cstdlib>
#include <vector>

using namespace tessera::md;

namespace {
RegionSetup detect(const char *country) {
  return detect_region(tessera::testrom::region_detect_rom(country));
}
constexpr RegionSetup kUsa{Region::usa, Standard::ntsc};
constexpr RegionSetup kJapan{Region::japan, Standard::ntsc};
constexpr RegionSetup kEurope{Region::europe, Standard::pal};
constexpr RegionSetup kJapanPal{Region::japan, Standard::pal};
} // namespace

TEST_CASE("country letters pick USA, then Japan, then Europe") {
  CHECK(detect("U") == kUsa);
  CHECK(detect("J") == kJapan);
  CHECK(detect("E") == kEurope);
  CHECK(detect("K") == kJapan); // Korea
  CHECK(detect("UE") == kUsa);
  CHECK(detect("JU") == kUsa);
  CHECK(detect("JE") == kJapan);
  CHECK(detect("JUE") == kUsa);
  CHECK(detect("j") == kJapan); // lower case counts
}

TEST_CASE("country as a hexadecimal code: 1 Japan, 2 Japan PAL, 4 USA, 8 EU") {
  CHECK(detect("1") == kJapan);
  CHECK(detect("2") == kJapanPal);
  CHECK(detect("4") == kUsa);
  CHECK(detect("8") == kEurope);
  CHECK(detect("A") == kEurope); // 10 = Europe + Japan PAL
  CHECK(detect("C") == kUsa);    // 12 = Europe + USA
  CHECK(detect("5") == kUsa);    // 1 + 4
  CHECK(detect("3") == kJapan);  // 1 + 2
}

TEST_CASE("country as a word") {
  CHECK(detect("eur") == kEurope);
  CHECK(detect("EUR") == kEurope);
  CHECK(detect("Europe") == kEurope);
  CHECK(detect("jap") == kJapan);
  CHECK(detect("usa") == kUsa);
}

TEST_CASE("an empty or unknown country is USA") {
  CHECK(detect("") == kUsa);
  CHECK(detect("0") == kUsa);
  CHECK(detect("Z") == kUsa);
}

TEST_CASE("cartridges with a wrong header are recognised") {
  using tessera::testrom::region_detect_rom;
  // Back to the Future III names USA but is a PAL cartridge.
  CHECK(detect_region(region_detect_rom("U", " T-69046-50")) == kEurope);
  CHECK(detect_region(region_detect_rom("U", " T-120106-00")) == kEurope);
  // Alisia Dragon only with its checksum.
  CHECK(detect_region(region_detect_rom("U", " T-45033 -00", 0x0F81)) ==
        kEurope);
  CHECK(detect_region(region_detect_rom("U", " T-45033 -00", 0x1234)) == kUsa);
  // A product code that merely resembles one.
  CHECK(detect_region(region_detect_rom("U", " T-69046-51")) == kUsa);
}

TEST_CASE("a short image is USA") {
  const std::vector<std::uint8_t> tiny(0x100, 0);
  CHECK(detect_region(tiny) == kUsa);
}

TEST_CASE("manual selection overrides the header") {
  const auto rom = tessera::testrom::region_detect_rom("U");
  CHECK(resolve_region(rom, RegionChoice::automatic) == kUsa);
  CHECK(resolve_region(rom, RegionChoice::usa) == kUsa);
  CHECK(resolve_region(rom, RegionChoice::europe) == kEurope);
  CHECK(resolve_region(rom, RegionChoice::japan_ntsc) == kJapan);
  CHECK(resolve_region(rom, RegionChoice::japan_pal) == kJapanPal);
}

TEST_CASE("the region sets the version register bits") {
  auto version = [](const char *country, RegionChoice choice) {
    const auto rom = tessera::testrom::region_detect_rom(country);
    System system(rom, config_for_rom(rom, choice));
    system.reset();
    return system.io_version();
  };
  // bit 7 overseas, bit 6 PAL, bit 5 no expansion unit
  CHECK(version("U", RegionChoice::automatic) == 0xA0);
  CHECK(version("J", RegionChoice::automatic) == 0x20);
  CHECK(version("E", RegionChoice::automatic) == 0xE0);
  CHECK(version("2", RegionChoice::automatic) == 0x60);
  CHECK(version("U", RegionChoice::europe) == 0xE0);
  CHECK(version("E", RegionChoice::usa) == 0xA0);
}

TEST_CASE("PAL runs 313 lines per frame, NTSC 262") {
  auto cycles_per = [](const char *country) {
    const auto rom = tessera::testrom::region_detect_rom(country);
    System system(rom, config_for_rom(rom));
    system.reset();
    system.run_frame();
    const auto before = system.cpu().getClock();
    system.run_frame();
    return static_cast<long long>(system.cpu().getClock() - before);
  };
  // Master cycles per frame: 262 * 3420 and 313 * 3420; the 68000 takes one
  // cycle in seven of them (an instruction may end a few cycles late).
  CHECK(std::llabs(cycles_per("U") - (262LL * 3420) / 7) < 40);
  CHECK(std::llabs(cycles_per("E") - (313LL * 3420) / 7) < 40);
}

TEST_CASE("the probe sees the version register and the frame count") {
  const auto rom = tessera::testrom::region_detect_rom("E");
  System system(rom, config_for_rom(rom));
  system.reset();
  for (int i = 0; i < 10; ++i)
    system.run_frame();
  const auto ram = system.work_ram();
  CHECK(ram[3] >= 9);    // V-ints counted, big-endian long at $FF0000
  CHECK(ram[4] == 0xE0); // version register
  const auto lines = [&](int n) {
    return (ram[0x10 + 4 * n + 2] << 8) | ram[0x10 + 4 * n + 3];
  };
  CHECK(lines(3) - lines(2) == 313); // lines per frame
}
