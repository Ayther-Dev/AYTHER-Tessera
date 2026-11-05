// TP-107.3 (RF-2.3): the PSG of the VDP. Latch and data bytes, tone square
// waves, attenuation steps and the two noise modes.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/psg.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <set>

using namespace tessera::md;

namespace {
void run(Psg &psg, unsigned samples) {
  psg.run_to((psg.sample_count() + samples) * Psg::kMasterPerSample);
}
std::uint64_t now(const Psg &psg) {
  return psg.sample_count() * Psg::kMasterPerSample;
}
unsigned toggles(const Psg &psg) {
  unsigned n = 0;
  const auto s = psg.samples();
  for (std::size_t i = 1; i < s.size(); ++i)
    if ((s[i] > 0) != (s[i - 1] > 0))
      ++n;
  return n;
}
int peak(const Psg &psg) {
  int p = 0;
  for (const auto s : psg.samples())
    p = std::max(p, std::abs(static_cast<int>(s)));
  return p;
}
} // namespace

TEST_CASE("a latch byte sets the low bits and a data byte the high ones") {
  Psg psg;
  psg.write(0x8A, 0); // channel 0 tone, low 4 bits = 0xA
  CHECK(psg.tone_period(0) == 0x00A);
  psg.write(0x15, 0); // data: bits 9-4 = 0x15
  CHECK(psg.tone_period(0) == 0x15A);
  psg.write(0xC3, 0); // channel 2 tone low
  psg.write(0x01, 0); // a data byte follows the latched channel 2
  CHECK(psg.tone_period(2) == 0x013);
  CHECK(psg.tone_period(0) == 0x15A);
}

TEST_CASE("attenuation: the latch byte and a following data byte") {
  Psg psg;
  CHECK(psg.attenuation(1) == 15); // silent after reset
  psg.write(0xB2, 0);              // channel 1 attenuation 2
  CHECK(psg.attenuation(1) == 2);
  psg.write(0x05, 0); // data byte for the latched attenuation
  CHECK(psg.attenuation(1) == 5);
  psg.write(0xFF, 0);
  CHECK(psg.attenuation(3) == 15);
}

TEST_CASE("a tone flips every period and a silent channel stays at zero") {
  Psg psg;
  run(psg, 100);
  for (const auto s : psg.samples())
    CHECK(s == 0);
  psg.clear_samples();
  psg.write(0x80, now(psg));
  psg.write(0x02, now(psg)); // period 0x20
  psg.write(0x90, now(psg)); // loudest
  run(psg, 640);
  CHECK(toggles(psg) >= 18);
  CHECK(toggles(psg) <= 21); // 640 / 32 = 20 half periods
}

TEST_CASE("two attenuation steps are about 4 dB apart") {
  Psg psg;
  psg.write(0x80, 0);
  psg.write(0x01, 0);
  psg.write(0x90, 0);
  run(psg, 64);
  const int loud = peak(psg);
  psg.clear_samples();
  psg.write(0x92, now(psg));
  run(psg, 64);
  const int soft = peak(psg);
  CHECK(loud > soft);
  CHECK(soft * 100 / loud >= 55);
  CHECK(soft * 100 / loud <= 70);
}

TEST_CASE("a noise write restarts the register; modes differ") {
  Psg white, periodic;
  white.write(0xE4, 0);    // white noise, rate 0
  periodic.write(0xE0, 0); // periodic noise, rate 0
  CHECK(white.lfsr() == 0x8000);
  CHECK(periodic.lfsr() == 0x8000);
  white.write(0xF0, 0);
  periodic.write(0xF0, 0);
  run(white, 4000);
  run(periodic, 4000);
  CHECK(white.lfsr() != periodic.lfsr());
  std::set<std::uint16_t> seen;
  Psg p2;
  p2.write(0xE0, 0);
  for (unsigned i = 0; i < 40; ++i) {
    run(p2, 32);
    seen.insert(p2.lfsr());
  }
  CHECK(seen.size() > 4);
}
