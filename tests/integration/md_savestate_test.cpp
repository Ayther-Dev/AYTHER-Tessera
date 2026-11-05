// TP-110.1 (RF-4.1, RF-4.2): savestate of the native Mega Drive. Run K frames,
// save, run N more and keep a digest of every frame (video, audio, RAM, VDP
// memories); restore the state into a new machine and run the same N frames:
// every digest must match. The state of a restored machine saves back to the
// same bytes and is at most 2 MiB (RNF-2). A truncated, corrupted or foreign
// state is refused without crashing.
//
// Synthetic ROMs always; commercial ROMs only when TESSERA_SAVESTATE_ROMS lists
// them (separated by '|'), run with more frames.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_digest.hpp"
#include "support/md_testrom.hpp"
#include "support/md_z80_testrom.hpp"
#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace tessera::md;
using tessera::testsupport::digest;

namespace {
using Rom = std::vector<std::uint8_t>;
using Machine = std::unique_ptr<System>; // a System is too big for the stack
Machine make(const Rom &rom, const MachineConfig &config) {
  return std::make_unique<System>(rom, config);
}
/// True if a fresh machine accepts `bytes`.
bool accepts(const Rom &rom, const MachineConfig &config,
             std::span<const std::uint8_t> bytes) {
  return make(rom, config)->load_state(bytes);
}

MachineConfig config_for(const Rom &rom, bool six_button) {
  auto config = MachineConfig::for_region(Region::usa);
  if (rom.size() > 0x1F0 && rom[0x1F0] == 'E')
    config = MachineConfig::for_region(Region::europe);
  if (six_button)
    config.pads = {PadType::six_button, PadType::six_button};
  return config;
}

void press(System &system, unsigned frame) {
  // A moving pad, so that the pad ports have history to save.
  system.set_pad(0, static_cast<std::uint16_t>(0x100 + (frame * 37) % 0xFF));
  system.set_pad(1, static_cast<std::uint16_t>((frame * 11) % 0xFFF));
}

void check_rom(const Rom &rom, unsigned k, unsigned n, bool six_button,
               const char *name) {
  CAPTURE(name);
  const auto config = config_for(rom, six_button);
  auto a = make(rom, config);
  a->reset();
  for (unsigned i = 0; i < k; ++i) {
    press(*a, i);
    a->run_frame();
  }
  const auto state = a->save_state();
  CHECK(state.size() > 0x10000);
  CHECK(state.size() <= 2U * 1024U * 1024U);

  // The original goes on.
  std::vector<std::uint64_t> original;
  for (unsigned i = 0; i < n; ++i) {
    press(*a, k + i);
    a->run_frame();
    original.push_back(digest(*a));
  }

  // A new machine takes the state and must go on identically.
  auto b = make(rom, config);
  REQUIRE(b->load_state(state));
  CHECK(b->save_state() == state); // idempotent
  std::vector<std::uint64_t> restored;
  for (unsigned i = 0; i < n; ++i) {
    press(*b, k + i);
    b->run_frame();
    restored.push_back(digest(*b));
  }
  CHECK(restored == original);
  CHECK(b->save_state() == a->save_state());

  // Over a machine that already ran (and was reset): the same.
  auto c = make(rom, config);
  c->reset();
  for (unsigned i = 0; i < 3; ++i)
    c->run_frame();
  REQUIRE(c->load_state(state));
  CHECK(c->save_state() == state);
}

Rom load_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return Rom((std::istreambuf_iterator<char>(in)),
             std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("savestate: K, restore, N is identical on synthetic ROMs") {
  using namespace tessera::testrom;
  check_rom(vint_counter_rom(), 12, 12, false, "vint_counter");
  check_rom(region_detect_rom("E"), 12, 12, false, "region_detect (PAL)");
  check_rom(pad_frames_rom(), 12, 12, true, "pad_frames (six-button)");
  check_rom(pad_latch_rom(3), 12, 12, true, "pad_latch");
  check_rom(fm_psg_sweep_rom(0), 12, 12, false, "fm_psg_sweep");
  check_rom(dma_fifo_rom(), 8, 8, false, "dma_fifo");
  check_rom(hint_counter_rom(9), 12, 12, false, "hint_counter");
  check_rom(z80_clock_rom(false), 12, 12, false, "z80_clock");
  check_rom(z80_int_rom(0), 12, 12, false, "z80_int");
  check_rom(bus_sweep_rom(), 6, 6, false, "bus_sweep (SRAM)");
  check_rom(ssf2_sweep_rom(), 6, 6, false, "ssf2_sweep (mapper)");
  check_rom(mode4_pbc_rom(), 10, 10, false, "mode4_pbc");
  check_rom(sprite_limits_rom(sprite_variant(1)), 8, 8, false, "sprite_limits");
  check_rom(modes_sweep_rom(modes_variant(1)), 8, 8, false, "modes_sweep");
}

TEST_CASE("savestate: a truncated, corrupted or foreign state is refused") {
  const auto rom = tessera::testrom::vint_counter_rom();
  const auto config = config_for(rom, false);
  auto a = make(rom, config);
  a->reset();
  for (int i = 0; i < 5; ++i)
    a->run_frame();
  const auto state = a->save_state();
  const std::span<const std::uint8_t> all(state);

  for (const std::size_t cut :
       {std::size_t{0}, std::size_t{3}, std::size_t{15}, std::size_t{100},
        state.size() / 2, state.size() - 5, state.size() - 1})
    CHECK_FALSE(accepts(rom, config, all.first(cut)));
  auto longer = state;
  longer.push_back(0);
  CHECK_FALSE(accepts(rom, config, longer));

  auto bad_magic = state;
  bad_magic[0] ^= 0xFF;
  CHECK_FALSE(accepts(rom, config, bad_magic));
  auto bad_version = state;
  bad_version[4] = 2;
  CHECK_FALSE(accepts(rom, config, bad_version));

  // Another ROM, another region, another pad type.
  CHECK_FALSE(accepts(tessera::testrom::pad_frames_rom(), config, state));
  CHECK_FALSE(accepts(rom, MachineConfig::for_region(Region::europe), state));
  auto six = config;
  six.pads = {PadType::six_button, PadType::three_button};
  CHECK_FALSE(accepts(rom, six, state));

  // Flipping any single byte never crashes; header bytes are refused.
  for (std::size_t at = 0; at < state.size(); at += 997) {
    auto flipped = state;
    flipped[at] ^= 0x5A;
    (void)accepts(rom, config, flipped);
  }
  for (std::size_t at = 0; at < 20; ++at) {
    auto flipped = state;
    flipped[at] ^= 0x01;
    CHECK_FALSE(accepts(rom, config, flipped));
  }
}

TEST_CASE("savestate: commercial ROMs from TESSERA_SAVESTATE_ROMS") {
  const char *list = std::getenv("TESSERA_SAVESTATE_ROMS");
  if (list == nullptr || *list == '\0')
    return;
  std::stringstream paths(list);
  for (std::string path; std::getline(paths, path, '|');) {
    if (path.empty())
      continue;
    const auto rom = load_file(path);
    REQUIRE_MESSAGE(!rom.empty(), path);
    check_rom(rom, 400, 120, false, path.c_str());
  }
}
