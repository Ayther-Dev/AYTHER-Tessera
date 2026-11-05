// TP-108.4 (RF-2.4, PQ-13): console region from the ROM header and by hand.
// The version register and the lines per frame stored by the `region_detect`
// ROM are the values gpgx publishes for the same header (the bench compares
// them in integration.md_region-*; those observations are listed in
// docs/clean-room-register.md).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/md_backend.hpp"
#include "md/region.hpp"
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <array>
#include <string_view>
#include <vector>

using namespace tessera::md;

namespace {
std::array<std::uint8_t, 16> field(std::string_view text) {
  std::array<std::uint8_t, 16> out;
  out.fill(' ');
  for (std::size_t i = 0; i < text.size() && i < out.size(); ++i)
    out[i] = static_cast<std::uint8_t>(text[i]);
  return out;
}

struct Seen {
  std::uint8_t version;
  unsigned lines;
};

/// Runs `region_detect` on a machine configured as `choice` says.
Seen run(std::string_view country, RegionChoice choice) {
  const auto rom = tessera::testrom::region_detect_rom(field(country));
  System system(rom, region_config(choice, rom));
  system.reset();
  for (int i = 0; i < 30; ++i)
    system.run_frame();
  const auto ram = system.work_ram();
  return {ram[4], static_cast<unsigned>(ram[6] << 8 | ram[7])};
}

constexpr std::uint8_t kUsa = 0xA0, kJapan = 0x20, kEurope = 0xE0,
                       kJapanPal = 0x60;
} // namespace

TEST_CASE("letters: Americas wins over Japan, Japan over Europe") {
  const struct {
    std::string_view country;
    std::uint8_t version;
    unsigned lines;
  } table[] = {
      {"J", kJapan, 262},    {"U", kUsa, 262},      {"E", kEurope, 313},
      {"JU", kUsa, 262},     {"JE", kJapan, 262},   {"UE", kUsa, 262},
      {"EJ", kJapan, 262},   {"JUE", kUsa, 262},    {"EUJ", kUsa, 262},
      {"j", kJapan, 262},    {"u", kUsa, 262},      {"e", kEurope, 313},
      {"  E", kEurope, 313}, {"   E", kEurope, 313}, {"    E", kUsa, 262},
      {"JX", kJapan, 262},   {"XE", kEurope, 313},  {"X", kUsa, 262},
      {"", kUsa, 262},
  };
  for (const auto &row : table) {
    CAPTURE(row.country);
    const auto seen = run(row.country, RegionChoice::automatic);
    CHECK(seen.version == row.version);
    CHECK(seen.lines == row.lines);
  }
}

TEST_CASE("hex digit: bit 0 Japan, bit 1 Japan PAL, bit 2 Americas, bit 3 "
          "Europe") {
  const struct {
    std::string_view country;
    std::uint8_t version;
    unsigned lines;
  } table[] = {
      {"0", kUsa, 262},   {"1", kJapan, 262},    {"2", kJapanPal, 313},
      {"3", kJapan, 262}, {"4", kUsa, 262},      {"8", kEurope, 313},
      {"9", kJapan, 262}, {"A", kEurope, 313},   {"B", kJapan, 262},
      {"C", kUsa, 262},   {"D", kUsa, 262},      {"F", kUsa, 262},
      {"a", kEurope, 313}, {"b", kJapan, 262},   {"2 U", kUsa, 262},
      {"2E", kEurope, 313}, {"1E", kJapan, 262}, {"8U", kUsa, 262},
      {"    2", kUsa, 262},
  };
  for (const auto &row : table) {
    CAPTURE(row.country);
    const auto seen = run(row.country, RegionChoice::automatic);
    CHECK(seen.version == row.version);
    CHECK(seen.lines == row.lines);
  }
}

TEST_CASE("a manual choice ignores the header") {
  CHECK(run("J", RegionChoice::europe).version == kEurope);
  CHECK(run("J", RegionChoice::europe).lines == 313);
  CHECK(run("E", RegionChoice::usa).version == kUsa);
  CHECK(run("E", RegionChoice::usa).lines == 262);
  CHECK(run("U", RegionChoice::japan).version == kJapan);
  CHECK(run("U", RegionChoice::japan_pal).version == kJapanPal);
  CHECK(run("U", RegionChoice::japan_pal).lines == 313);
}

TEST_CASE("an image too short for the field falls back to the Americas") {
  const std::vector<std::uint8_t> shorter(0x100, 'E');
  const auto config = region_from_header(shorter);
  CHECK(config.region == Region::usa);
  CHECK(config.standard == Standard::ntsc);
}

TEST_CASE("the backend follows the header and the caller's choice") {
  const std::vector<std::uint8_t> memory(65536, 0);
  const auto pal = tessera::testrom::region_detect_rom(field("E"));
  const auto ntsc = tessera::testrom::region_detect_rom(field("U"));
  auto backend = make_backend("megadrive", pal, memory);
  CHECK(backend->frame_ticks() == 313ULL * kCyclesPerLine);
  CHECK(backend->tick_hz() == master_hz(Standard::pal));
  backend = make_backend("megadrive", ntsc, memory);
  CHECK(backend->frame_ticks() == 262ULL * kCyclesPerLine);
  backend = make_backend("megadrive", ntsc, memory, RegionChoice::europe);
  CHECK(backend->frame_ticks() == 313ULL * kCyclesPerLine);
  backend = make_backend("megadrive", pal, memory, RegionChoice::japan);
  CHECK(backend->frame_ticks() == 262ULL * kCyclesPerLine);
}
