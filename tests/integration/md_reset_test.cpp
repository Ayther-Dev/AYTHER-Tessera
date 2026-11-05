// TP-110.4 (RF-4.4): hard and soft reset of the native Mega Drive.
//  - Hard reset: a machine that ran and was reset behaves exactly like a new
//  one.
//  - Soft reset (the RESET button): the 68000 starts again from its vectors and
//    the Z80 bus is released, while work RAM, the VDP and the clock are kept.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_digest.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <cstring>
#include <memory>
#include <vector>

using namespace tessera::md;
using tessera::testsupport::digest;

namespace {
using Rom = std::vector<std::uint8_t>;
using Machine = std::unique_ptr<System>;

Machine make(const Rom &rom) {
  return std::make_unique<System>(rom, MachineConfig::for_region(Region::usa));
}

void put32(Rom &rom, std::size_t at, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    rom[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
}

/// Boot counter: every start of the program adds one to $FF0100, then the
/// 68000 requests the Z80 bus and loops.
Rom boot_counter_rom() {
  const std::vector<std::uint16_t> code = {
      0x52B9, 0x00FF, 0x0100, // addq.l #1,$FF0100
      0x33FC, 0x0100, 0x00A1,
      0x1200, // move.w #$100,$A11200: Z80 reset released
      0x33FC, 0x0100, 0x00A1,
      0x1100, // move.w #$100,$A11100: bus request
      0x60FE, // bra *
  };
  Rom rom(0x200 + code.size() * 2 + 2, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memcpy(&rom[0x120], "AYTHER BOOT CNT ", 16);
  put32(rom, 0x1A4, static_cast<std::uint32_t>(rom.size() - 1));
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::size_t at = 0x200;
  for (const auto word : code) {
    rom[at] = static_cast<std::uint8_t>(word >> 8);
    rom[at + 1] = static_cast<std::uint8_t>(word);
    at += 2;
  }
  return rom;
}

std::uint32_t boots(const System &s) {
  const auto ram = s.work_ram();
  return (std::uint32_t{ram[0x100]} << 24) | (std::uint32_t{ram[0x101]} << 16) |
         (std::uint32_t{ram[0x102]} << 8) | ram[0x103];
}

std::vector<std::uint64_t> frames(System &s, unsigned n) {
  std::vector<std::uint64_t> out;
  for (unsigned i = 0; i < n; ++i) {
    s.set_pad(0, static_cast<std::uint16_t>(0x100 + (i * 37) % 0xFF));
    s.run_frame();
    out.push_back(digest(s));
  }
  return out;
}
} // namespace

TEST_CASE("a hard reset gives a machine like a new one") {
  using namespace tessera::testrom;
  const std::vector<Rom> roms = {vint_counter_rom(),  pad_frames_rom(),
                                 fm_psg_sweep_rom(5), dma_fifo_rom(),
                                 hint_counter_rom(9), boot_counter_rom()};
  for (const auto &rom : roms) {
    auto fresh = make(rom);
    fresh->reset();
    const auto expected = frames(*fresh, 40);

    auto used = make(rom);
    used->reset();
    frames(*used, 33);
    used->reset();
    CHECK(frames(*used, 40) == expected);
    CHECK(used->save_state() == fresh->save_state());
  }
}

TEST_CASE("a soft reset restarts the program and keeps work RAM") {
  const auto rom = boot_counter_rom();
  auto s = make(rom);
  s->reset();
  frames(*s, 6);
  CHECK(boots(*s) == 1);
  CHECK(s->z80_bus_granted());

  s->soft_reset();
  CHECK_FALSE(s->z80_bus_granted()); // the bus is released with the reset
  frames(*s, 6);
  CHECK(boots(*s) == 2); // the RAM survived and the program ran again
  CHECK(s->z80_bus_granted());

  s->soft_reset();
  s->soft_reset(); // a second press before a frame runs
  frames(*s, 6);
  CHECK(boots(*s) == 3);

  // A hard reset on top clears it.
  s->reset();
  frames(*s, 6);
  CHECK(boots(*s) == 1);
}

TEST_CASE("a soft reset is deterministic and keeps the clock going") {
  const auto rom = boot_counter_rom();
  auto run = [&] {
    auto s = make(rom);
    s->reset();
    frames(*s, 10);
    s->soft_reset();
    return frames(*s, 20);
  };
  CHECK(run() == run());

  auto s = make(rom);
  s->reset();
  frames(*s, 10);
  const auto before = s->cpu().getClock();
  s->soft_reset();
  CHECK(s->cpu().getClock() >= before); // time does not go back
  CHECK(s->cpu().getClock() - before < 200);
}

TEST_CASE("a soft reset keeps the VDP, the PSG and the sound mute") {
  using namespace tessera::testrom;
  auto s = make(fm_psg_sweep_rom(5));
  s->reset();
  frames(*s, 8);
  const auto vram =
      std::vector<std::uint8_t>(s->vdp().vram().begin(), s->vdp().vram().end());
  const auto attenuation = s->psg().attenuation(0);
  s->set_audio_mute(0x41);
  s->soft_reset();
  CHECK(std::vector<std::uint8_t>(s->vdp().vram().begin(),
                                  s->vdp().vram().end()) == vram);
  CHECK(s->psg().attenuation(0) == attenuation);
  CHECK(s->audio_mute() == 0x41U);
}
