// TP-107.2 (RF-3.7, RNF-5): the sound chip write log of the native machine.
// The expected lists are the ones gpgx publishes for the same synthetic ROMs
// (integration.md_fm-psg-*): address writes are not logged, a data write carries
// the register and part of the last address write, the PSG has address 0, words
// reach the FM through their high byte and the PSG through their low byte, and
// the log holds the writes of the last frame only, up to 8 192.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include "support/md_z80_testrom.hpp"
#include <doctest/doctest.h>

#include <vector>

using namespace tessera::md;

namespace {
struct Entry {
  std::uint16_t addr;
  std::uint8_t data;
  std::uint8_t chip;
  bool operator==(const Entry &) const = default;
};

std::vector<Entry> log_of(const System &system) {
  std::vector<Entry> out;
  for (const auto &w : system.audio_writes())
    out.push_back({w.addr, w.data, w.chip});
  return out;
}

System make(std::vector<std::uint8_t> rom) {
  System system(std::move(rom), MachineConfig::for_region(Region::usa));
  system.reset();
  return system;
}
} // namespace

TEST_CASE("FM writes: data writes only, one latch for the part") {
  auto system = make(tessera::testrom::fm_psg_sweep_rom(0, true));
  system.run_frame();
  const std::vector<Entry> expected = {
      {0x022, 0x08, 0}, {0x027, 0x00, 0}, {0x028, 0xF0, 0}, {0x028, 0x55, 0},
      {0x11A, 0x77, 0}, {0x11A, 0x66, 0}, {0x02A, 0x80, 0}, {0x02B, 0x80, 0},
      {0x0B4, 0xC0, 0}, {0x0A4, 0x22, 0}, {0x0A0, 0x69, 0}, {0x1B4, 0xC0, 0},
  };
  CHECK(log_of(system) == expected);
}

TEST_CASE("PSG writes: low byte lane, address 0, chip 1") {
  auto system = make(tessera::testrom::fm_psg_sweep_rom(1, true));
  system.run_frame();
  const std::vector<std::uint8_t> data = {0x8F, 0x3F, 0x90, 0xA5, 0xB3, 0xE5,
                                          0xF1, 0xA7, 0x84, 0xC2, 0x01, 0x9F};
  const auto log = log_of(system);
  REQUIRE(log.size() == data.size());
  for (std::size_t i = 0; i < data.size(); ++i)
    CHECK(log[i] == Entry{0, data[i], 1});
}

TEST_CASE("word writes reach the FM address port and are not logged") {
  auto system = make(tessera::testrom::fm_psg_sweep_rom(3, true));
  system.run_frame();
  const std::vector<Entry> expected = {{0x022, 0x99, 0},
                                       {0x027, 0x98, 0},
                                       {0x11A, 0x97, 0},
                                       {0x130, 0x96, 0},
                                       {0x02B, 0x95, 0}};
  CHECK(log_of(system) == expected);
}

TEST_CASE("the Z80 writes the chips too") {
  auto system = make(tessera::testrom::fm_psg_z80_rom());
  system.run_frame();
  const std::vector<Entry> expected = {
      {0x022, 0x08, 0}, {0x028, 0xF0, 0}, {0x11A, 0x77, 0}, {0x02B, 0x80, 0},
      {0x02B, 0x80, 0}, {0x02A, 0x81, 0}, {0x02A, 0x66, 0}, {0, 0x9F, 1},
      {0, 0x8F, 1},     {0, 0xE4, 1},     {0, 0xBF, 1},     {0, 0x91, 1},
  };
  CHECK(log_of(system) == expected);
}

TEST_CASE("the log holds the last frame only") {
  auto system = make(tessera::testrom::fm_psg_sweep_rom(0, true));
  system.run_frame();
  CHECK(system.audio_writes().size() == 12);
  // The list is longer than the frame: the rest comes in the next one, and the
  // log has been emptied in between.
  system.run_frame();
  const std::vector<Entry> rest = {{0x028, 0x00, 0}, {0x021, 0x01, 0},
                                   {0x024, 0x80, 0}, {0x025, 0x02, 0},
                                   {0x027, 0x15, 0}};
  CHECK(log_of(system) == rest);
  system.run_frame(); // the 68000 idles
  CHECK(system.audio_writes().empty());
}

TEST_CASE("past 8 192 writes in a frame the first ones stay and the rest count") {
  auto system = make(tessera::testrom::fm_psg_sweep_rom(4));
  for (int i = 0; i < 8; ++i)
    system.run_frame();
  CHECK(system.audio_writes().size() == kAudioWriteCapacity);
  CHECK(system.audio_writes_dropped() > 0);
  CHECK(system.audio_writes().front().data == 0x80);
  CHECK(system.audio_writes()[1].data == 0x81);
  // cycles never go backwards
  std::uint32_t last = 0;
  for (const auto &w : system.audio_writes()) {
    CHECK(w.cycle >= last);
    last = w.cycle;
  }
}

TEST_CASE("the chips keep their own time: one YM sample per 1008 master cycles") {
  auto system = make(tessera::testrom::fm_psg_sweep_rom(0, true));
  for (int i = 0; i < 10; ++i)
    system.run_frame();
  CHECK(system.ym().sample_count() == 10ULL * cycles_per_frame(Standard::ntsc) /
                                          Ym2612::kMasterPerSample);
  CHECK(system.psg().sample_count() ==
        10ULL * cycles_per_frame(Standard::ntsc) / Psg::kMasterPerSample);
}
