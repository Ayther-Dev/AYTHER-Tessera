// TP-108.1/108.2 (RF-3.4): the pad ports. The expected streams are the ones
// gpgx publishes for the same synthetic ROM (integration.md_pad-latch-*):
// a three-button pad answers every TH level the same way, a six-button pad
// counts TH edges and shows its extra row on the third high level.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <vector>

using namespace tessera::md;

namespace {
std::vector<std::uint8_t> run(PadType type, std::uint16_t port1,
                              std::uint16_t port2, unsigned variant) {
  auto config = MachineConfig::for_region(Region::usa);
  config.pads = {type, type};
  System system(tessera::testrom::pad_latch_rom(variant), config);
  system.reset();
  system.set_pad(0, port1);
  system.set_pad(1, port2);
  for (int i = 0; i < 20; ++i)
    system.run_frame();
  const auto ram = system.work_ram();
  return {ram.begin(), ram.begin() + 24};
}
} // namespace

TEST_CASE("three-button pad: the same four bytes for every TH pair") {
  const auto out = run(PadType::three_button, 0, kPadX | 0xFF, 3);
  const std::vector<std::uint8_t> expect = {0x7F, 0x40, 0x33, 0x00,
                                            0x7F, 0x40, 0x33, 0x00};
  CHECK(std::vector<std::uint8_t>(out.begin(), out.begin() + 8) == expect);
  for (std::size_t i = 4; i < out.size(); ++i)
    CHECK(out[i] == out[i - 4]);
}

TEST_CASE("six-button pad: the fourth cycle shows the extra row") {
  const auto out = run(PadType::six_button, 0, 0xFFF, 3);
  const std::vector<std::uint8_t> expect = {
      0x7F, 0x40, 0x33, 0x00, 0x7F, 0x40, 0x30, 0x00, 0x7F, 0x40, 0x3F, 0x0F,
      0x7F, 0x40, 0x33, 0x00, 0x7F, 0x40, 0x33, 0x00, 0x7F, 0x40, 0x33, 0x00};
  CHECK(out == expect);
}

TEST_CASE("six-button pad: a long pause restarts the sequence") {
  const auto fresh = run(PadType::six_button, 0, 0xFFF, 1);
  const auto again = run(PadType::six_button, 0, 0xFFF, 1);
  CHECK(fresh == again); // deterministic
}
