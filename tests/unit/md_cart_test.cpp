// TP-101.3 (RF-2.5, RF-4.5, PQ-13): cartridge loader of the native Mega Drive
// core: plain .bin/.md, 512 B copier header, interleaved .smd, size limit and
// header fields (region, SRAM).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "domain/result.hpp"
#include "md/cart.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <cstring>
#include <vector>

using namespace tessera;
using tessera::md::Cart;
using Bytes = std::vector<std::uint8_t>;

namespace {
void put32(Bytes &b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
}
// A 128 KiB ROM with a valid header, recognizable payload and given region.
Bytes make_rom(const char *region = "JUE", bool sram = false,
               std::size_t size = 0x20000) {
  Bytes rom(size);
  for (std::size_t i = 0; i < rom.size(); ++i)
    rom[i] = static_cast<std::uint8_t>(i * 31 + (i >> 8));
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  put32(rom, 0x1A0, 0);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(size - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  std::memcpy(&rom[0x1F0], region, std::strlen(region));
  if (sram) {
    rom[0x1B0] = 'R';
    rom[0x1B1] = 'A';
    rom[0x1B2] = 0xF8; // 8 bit, odd addresses
    rom[0x1B3] = 0x20;
    put32(rom, 0x1B4, 0x200001);
    put32(rom, 0x1B8, 0x203FFF);
  } else {
    std::memset(&rom[0x1B0], 0x20, 12);
  }
  return rom;
}
// Interleaved .smd: 16 KiB blocks, odd bytes in the first half, even in the
// second.
Bytes to_smd(const Bytes &rom) {
  Bytes out(512, 0);
  out[0] = static_cast<std::uint8_t>(rom.size() / 0x4000);
  out[1] = 0x03;
  out[8] = 0xAA;
  out[9] = 0xBB;
  for (std::size_t block = 0; block < rom.size(); block += 0x4000) {
    Bytes inter(0x4000);
    for (std::size_t i = 0; i < 0x2000; ++i) {
      inter[i] = rom[block + 2 * i + 1];
      inter[0x2000 + i] = rom[block + 2 * i];
    }
    out.insert(out.end(), inter.begin(), inter.end());
  }
  return out;
}
tessera_status status_of(const Bytes &bytes) {
  try {
    (void)md::load_cart(bytes);
  } catch (const Failure &f) {
    return f.code;
  }
  return TESSERA_OK;
}
} // namespace

TEST_CASE("plain .bin/.md is taken as is") {
  const auto rom = make_rom();
  const Cart cart = md::load_cart(rom);
  CHECK(cart.format == md::CartFormat::plain);
  CHECK(cart.rom == rom);
}

TEST_CASE("a 512 byte copier header is stripped") {
  const auto rom = make_rom();
  Bytes with_header(512, 0xAA);
  with_header.insert(with_header.end(), rom.begin(), rom.end());
  const Cart cart = md::load_cart(with_header);
  CHECK(cart.format == md::CartFormat::copier_header);
  CHECK(cart.rom == rom);
}

TEST_CASE(".smd is deinterleaved") {
  const auto rom = make_rom();
  const Cart cart = md::load_cart(to_smd(rom));
  CHECK(cart.format == md::CartFormat::smd);
  CHECK(cart.rom == rom);
}

TEST_CASE("region letters and hexadecimal digit") {
  CHECK(md::load_cart(make_rom("U")).regions == md::kRegionUsa);
  CHECK(md::load_cart(make_rom("JE")).regions ==
        (md::kRegionJapan | md::kRegionEurope));
  CHECK(md::load_cart(make_rom("JUE")).regions ==
        (md::kRegionJapan | md::kRegionUsa | md::kRegionEurope));
  CHECK(md::load_cart(make_rom("4")).regions == md::kRegionUsa);
  CHECK(md::load_cart(make_rom("C")).regions ==
        (md::kRegionUsa | md::kRegionEurope));
  CHECK(md::load_cart(make_rom("")).regions == 0);
}

TEST_CASE("SRAM header") {
  const auto none = md::load_cart(make_rom());
  CHECK_FALSE(none.sram.present);
  const auto cart = md::load_cart(make_rom("U", true));
  CHECK(cart.sram.present);
  CHECK(cart.sram.start == 0x200001);
  CHECK(cart.sram.end == 0x203FFF);
  CHECK(cart.sram.bus == md::SramBus::odd);
  CHECK(cart.sram.size_bytes == 0x2000);
}

TEST_CASE("sizes: up to 16 MiB, more is invalid content") {
  CHECK(status_of(make_rom("U", false, 0x1000000)) == TESSERA_OK);
  CHECK(status_of(make_rom("U", false, 0x1000002)) == TESSERA_INVALID_CONTENT);
  Bytes huge(0x1000000 + 512 + 0x4000, 0);
  CHECK(status_of(huge) == TESSERA_INVALID_CONTENT);
}

TEST_CASE("malformed input is invalid content") {
  CHECK(status_of({}) == TESSERA_INVALID_CONTENT);
  CHECK(status_of(Bytes(0x100, 0)) == TESSERA_INVALID_CONTENT);
  CHECK(status_of(Bytes(0x20000, 0x55)) == TESSERA_INVALID_CONTENT); // no SEGA
  auto odd = make_rom();
  odd.push_back(0);
  CHECK(status_of(odd) == TESSERA_INVALID_CONTENT); // odd length
}
