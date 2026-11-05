// TP-110.5 (RF-4.5, RF-4.2): malformed external inputs never crash the native
// Mega Drive and never leave it in a half loaded state.
//  - ROM: empty, tiny, truncated, with a corrupt header or random bytes: the
//    machine builds and runs frames (a CPU that executes garbage is not a
//    fault of the emulator).
//  - .srm: an oversize file is refused and leaves the image untouched.
//  - savestate: truncated, extended or corrupted states are refused, and a
//    refused load leaves the machine running exactly as before.
// The fuzz uses a fixed seed (reproducible).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_digest.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace tessera::md;
using tessera::testsupport::digest;

namespace {
using Rom = std::vector<std::uint8_t>;
using Machine = std::unique_ptr<System>;

struct Rng {
  std::uint64_t s;
  std::uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<std::uint32_t>(s >> 16);
  }
};

Machine make(const Rom &rom) {
  return std::make_unique<System>(rom, MachineConfig::for_region(Region::usa));
}
void run(System &s, unsigned frames) {
  for (unsigned i = 0; i < frames; ++i)
    s.run_frame();
}
} // namespace

TEST_CASE("a malformed ROM builds and runs without crashing") {
  const auto good = tessera::testrom::pad_frames_rom();
  std::vector<Rom> roms;
  roms.push_back({});
  roms.push_back({0});
  roms.push_back(Rom(7, 0xFF));
  roms.push_back(Rom(0x100, 0));
  roms.push_back(Rom(0x200, 0));
  roms.push_back(Rom(0x400, 0xFF));
  for (const std::size_t cut : {std::size_t{0x1FF}, std::size_t{0x201},
                                std::size_t{0x203}, good.size() - 1})
    roms.emplace_back(good.begin(),
                      good.begin() + static_cast<std::ptrdiff_t>(cut));
  Rng rng{0x7E55E4A01ULL};
  for (int n = 0; n < 12; ++n) {
    Rom random(0x200 + rng.next() % 0x8000);
    for (auto &b : random)
      b = static_cast<std::uint8_t>(rng.next());
    roms.push_back(std::move(random));
  }
  // The valid ROM with bytes corrupted in the header and in the code.
  for (int n = 0; n < 12; ++n) {
    Rom bad = good;
    for (int k = 0; k < 6; ++k)
      bad[rng.next() % bad.size()] ^= static_cast<std::uint8_t>(1 + rng.next());
    roms.push_back(std::move(bad));
  }
  for (const auto &rom : roms) {
    CAPTURE(rom.size());
    auto s = make(rom);
    s->reset();
    run(*s, 6);
    s->soft_reset();
    run(*s, 3);
    // Whatever it did, its state is still a valid state of that ROM.
    const auto state = s->save_state();
    auto t = make(rom);
    CHECK(t->load_state(state));
  }
}

TEST_CASE("an oversize .srm is refused and leaves the image alone") {
  Rom rom = tessera::testrom::bus_sweep_rom();
  auto s = make(rom);
  s->reset();
  const auto size = s->sram().image_size();
  if (size == 0)
    return; // no SRAM in this cartridge
  const auto before = s->sram().to_srm();
  CHECK_FALSE(s->sram().load_srm(Rom(size + 1, 0x5A)));
  CHECK(s->sram().to_srm() == before);
  CHECK(s->sram().load_srm(Rom(size / 2, 0x33)));
  CHECK(s->sram().load_srm(Rom{}));
}

TEST_CASE("a refused savestate leaves the machine running as before") {
  const auto rom = tessera::testrom::pad_frames_rom();
  auto donor = make(rom);
  donor->reset();
  run(*donor, 9);
  const auto state = donor->save_state();

  auto reference = make(rom);
  reference->reset();
  run(*reference, 5);
  auto subject = make(rom);
  subject->reset();
  run(*subject, 5);
  const auto saved = subject->save_state();

  Rng rng{0xF00DCAFE5ULL};
  const std::span<const std::uint8_t> all(state);
  unsigned refused = 0;
  // Truncations and extensions.
  for (std::size_t cut = 0; cut < state.size(); cut += state.size() / 41 + 1) {
    refused += !subject->load_state(all.first(cut));
  }
  auto longer = state;
  longer.insert(longer.end(), 5, 0xAA);
  refused += !subject->load_state(longer);
  // Random corruptions (some may happen to be valid values).
  for (int n = 0; n < 200; ++n) {
    auto bad = state;
    for (int k = 0, flips = 1 + rng.next() % 4; k < flips; ++k)
      bad[rng.next() % bad.size()] ^=
          static_cast<std::uint8_t>(1 + rng.next() % 255);
    const bool ok = subject->load_state(bad);
    if (!ok) {
      ++refused;
      // A refused load changes nothing that matters: the machine goes on as
      // the reference does.
      CHECK(subject->save_state() == saved);
    } else {
      REQUIRE(subject->load_state(saved)); // go back to the reference point
    }
  }
  CHECK(refused > 40);
  // After all that, one more frame equals the reference.
  CHECK(subject->save_state() == saved);
  subject->run_frame();
  reference->run_frame();
  CHECK(digest(*subject) == digest(*reference));
}

TEST_CASE("a corrupted state that is accepted never crashes the machine") {
  const auto rom = tessera::testrom::dma_fifo_rom();
  auto donor = make(rom);
  donor->reset();
  run(*donor, 6);
  const auto state = donor->save_state();
  Rng rng{0xBADC0DE77ULL};
  for (int n = 0; n < 150; ++n) {
    auto bad = state;
    for (int k = 0; k < 3; ++k)
      bad[rng.next() % bad.size()] = static_cast<std::uint8_t>(rng.next());
    auto s = make(rom);
    if (s->load_state(bad))
      run(*s, 3);
  }
}
