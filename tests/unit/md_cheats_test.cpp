// TP-111 (RF-4.3): Game Genie and Pro Action Replay codes. Decoding with known
// vectors, and the two ways a cheat acts: a cartridge patch (acts at once, goes
// at once) and a work RAM rewrite (every frame, stops when removed).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/cheats.hpp"
#include "md/system.hpp"
#include <doctest/doctest.h>

#include <cstring>
#include <vector>

using namespace tessera::md;

namespace {
using Rom = std::vector<std::uint8_t>;

void put32(Rom &rom, std::size_t at, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    rom[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
}

constexpr std::uint32_t kConstantAt = 0x300;
constexpr std::uint16_t kConstant = 0x1111;

/// Forever: move.w $000300.l,$FF0200.l ; bra loop. The word at $300 is the
/// constant the loop copies into work RAM.
Rom copy_loop_rom() {
  const std::vector<std::uint16_t> code = {
      0x33F9, 0x0000, 0x0300, 0x00FF, 0x0200, // move.w $300.l,$FF0200.l
      0x60F4,                                 // bra.s to the move
  };
  Rom rom(0x400, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER CHEATS   ", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : code) {
    rom[at] = static_cast<std::uint8_t>(word >> 8);
    rom[at + 1] = static_cast<std::uint8_t>(word);
    at += 2;
  }
  rom[kConstantAt] = kConstant >> 8;
  rom[kConstantAt + 1] = kConstant & 0xFF;
  return rom;
}

System make() {
  return System(copy_loop_rom(), MachineConfig::for_region(Region::usa));
}

std::uint16_t ram_word(const System &s, std::size_t at) {
  const auto ram = s.work_ram();
  return static_cast<std::uint16_t>((ram[at] << 8) | ram[at + 1]);
}
} // namespace

TEST_CASE("Game Genie decoding") {
  // RFAA-A6VR is the well known code for a NOP ($4E71) at $00402E.
  const auto nop = decode_game_genie("RFAA-A6VR");
  REQUIRE(nop.has_value());
  CHECK(nop->address == 0x00402E);
  CHECK(nop->data == 0x4E71);

  SUBCASE("hyphen, case and look-alike letters") {
    CHECK(decode_game_genie("RFAAA6VR")->address == 0x00402E);
    CHECK(decode_game_genie("rfaa-a6vr")->data == 0x4E71);
    // 'O' reads as 0 and 'I' as 1
    CHECK(decode_game_genie("OAAA-AAAA").has_value() ==
          decode_game_genie("0AAA-AAAA").has_value());
  }
  SUBCASE("malformed codes") {
    CHECK_FALSE(decode_game_genie("").has_value());
    CHECK_FALSE(decode_game_genie("RFAA-A6V").has_value());   // short
    CHECK_FALSE(decode_game_genie("RFAA-A6VRA").has_value()); // long
    CHECK_FALSE(decode_game_genie("RFA-AA6VR").has_value()); // hyphen misplaced
    CHECK_FALSE(decode_game_genie("RFAA-A6V!").has_value()); // not a letter
    CHECK_FALSE(decode_game_genie("RFAA-A6VQ").has_value()); // Q is not used
  }
}

TEST_CASE("Pro Action Replay decoding") {
  const auto ram = decode_par("FF0100 BEEF");
  REQUIRE(ram.has_value());
  CHECK(ram->address == 0xFF0100);
  CHECK(ram->data == 0xBEEF);
  CHECK(cheat_targets_ram(*ram));

  const auto rom = decode_par("000300:2222");
  REQUIRE(rom.has_value());
  CHECK(rom->address == 0x000300);
  CHECK(rom->data == 0x2222);
  CHECK_FALSE(cheat_targets_ram(*rom));

  CHECK(decode_par("ff01002222")->address == 0xFF0100);

  SUBCASE("refused") {
    CHECK_FALSE(decode_par("").has_value());
    CHECK_FALSE(decode_par("FF0100").has_value());       // no data
    CHECK_FALSE(decode_par("FF0100 BEEF0").has_value()); // too long
    CHECK_FALSE(decode_par("FF0101 BEEF").has_value());  // odd address
    CHECK_FALSE(decode_par("A00000 BEEF").has_value());  // Z80 area
    CHECK_FALSE(decode_par("C00004 0000").has_value());  // VDP
    CHECK_FALSE(decode_par("GG0100 BEEF").has_value());  // not hexadecimal
    CHECK_FALSE(decode_par("FF0100 BEE").has_value());
  }
}

TEST_CASE("decode_cheat picks the family") {
  CHECK(decode_cheat("RFAA-A6VR")->data == 0x4E71);
  CHECK(decode_cheat("FF0100 BEEF")->data == 0xBEEF);
  CHECK_FALSE(decode_cheat("not a code").has_value());
}

TEST_CASE("a cartridge cheat acts at once and goes at once") {
  auto s = make();
  s.reset();
  s.run_frame();
  REQUIRE(ram_word(s, 0x200) == kConstant);

  const auto id = s.add_cheat("000300 2222");
  REQUIRE(id != 0);
  CHECK(s.read16(kConstantAt) == 0x2222);
  CHECK(s.read8(kConstantAt) == 0x22);
  CHECK(s.read8(kConstantAt + 1) == 0x22);
  CHECK(s.read16(kConstantAt + 2) == 0); // the word next to it is untouched
  s.run_frame();
  CHECK(ram_word(s, 0x200) == 0x2222);

  CHECK(s.remove_cheat(id));
  CHECK(s.read16(kConstantAt) == kConstant);
  s.run_frame();
  CHECK(ram_word(s, 0x200) == kConstant);
  CHECK_FALSE(s.remove_cheat(id));
}

TEST_CASE("a work RAM cheat rewrites every frame until it is removed") {
  auto s = make();
  s.reset();
  s.run_frame();
  REQUIRE(ram_word(s, 0x200) == kConstant);

  const auto id = s.add_cheat("FF0200 BEEF");
  REQUIRE(id != 0);
  for (int i = 0; i < 3; ++i) {
    s.run_frame();
    CHECK(ram_word(s, 0x200) == 0xBEEF); // the loop rewrites it, the cheat wins
  }
  CHECK(s.remove_cheat(id));
  s.run_frame();
  CHECK(ram_word(s, 0x200) == kConstant);
}

TEST_CASE("session controls: refusals, several cheats, reset and restore") {
  auto s = make();
  s.reset();
  CHECK(s.add_cheat("rubbish") == 0);
  CHECK(s.add_cheat("FF0101 0000") == 0);
  CHECK(s.cheat_count() == 0);

  const auto a = s.add_cheat("000300 2222");
  const auto b = s.add_cheat("FF0300 0102");
  CHECK(a != 0);
  CHECK(b != 0);
  CHECK(a != b);
  CHECK(s.cheat_count() == 2);

  // not in the savestate, and they survive a restore and a reset
  const auto state = s.save_state();
  REQUIRE(s.load_state(state));
  CHECK(s.cheat_count() == 2);
  s.reset();
  CHECK(s.cheat_count() == 2);
  CHECK(s.read16(kConstantAt) == 0x2222);

  s.clear_cheats();
  CHECK(s.cheat_count() == 0);
  CHECK(s.read16(kConstantAt) == kConstant);
}
