// TP-107.1 (RF-2.3): ymfm behind the native YM2612 adapter. A key-on produces
// samples, the timers fire at the cycle the registers say, and the chip is a
// pure function of its (cycle, access) sequence.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/ym.hpp"
#include <doctest/doctest.h>

#include <cstdlib>

using namespace tessera::md;

namespace {
constexpr std::uint64_t kStep = 100; // master cycles between two writes

void reg(Ym2612 &ym, std::uint64_t &now, unsigned part, unsigned r,
         unsigned v) {
  ym.write(part * 2U, static_cast<std::uint8_t>(r), now);
  now += kStep;
  ym.write(part * 2U + 1U, static_cast<std::uint8_t>(v), now);
  now += kStep;
}

// Channel 1, algorithm 7 (four carriers), full level, instant attack.
void program_voice(Ym2612 &ym, std::uint64_t &now) {
  for (unsigned op = 0; op < 4; ++op) {
    reg(ym, now, 0, 0x30 + op * 4, 0x01); // DT 0, MUL 1
    reg(ym, now, 0, 0x40 + op * 4, 0x00); // TL 0
    reg(ym, now, 0, 0x50 + op * 4, 0x1F); // AR 31
    reg(ym, now, 0, 0x60 + op * 4, 0x00);
    reg(ym, now, 0, 0x70 + op * 4, 0x00);
    reg(ym, now, 0, 0x80 + op * 4, 0x0F); // RR 15
  }
  reg(ym, now, 0, 0xB0, 0x07);
  reg(ym, now, 0, 0xB4, 0xC0); // both outputs
  reg(ym, now, 0, 0xA4, 0x22);
  reg(ym, now, 0, 0xA0, 0x69);
}

// Spread of the output: the DAC of the YM2612 has a constant offset at rest
// (the "ladder effect" discontinuity), so silence is a flat stream.
std::int64_t energy(const Ym2612 &ym) {
  const auto s = ym.samples();
  if (s.empty())
    return 0;
  std::int64_t sum = 0;
  for (const auto &x : s)
    sum += std::llabs(x.left - s[0].left) + std::llabs(x.right - s[0].right);
  return sum;
}
} // namespace

TEST_CASE("a key-on produces samples and silence before it does not") {
  Ym2612 ym;
  std::uint64_t now = 0;
  program_voice(ym, now);
  ym.run_to(now + 20 * Ym2612::kMasterPerSample);
  CHECK(ym.samples().size() >= 20);
  CHECK(energy(ym) == 0);
  ym.clear_samples();
  reg(ym, now, 0, 0x28, 0xF0); // key on, channel 1, all operators
  ym.run_to(now + 400 * Ym2612::kMasterPerSample);
  CHECK(energy(ym) > 0);
  ym.clear_samples();
  reg(ym, now, 0, 0x28, 0x00); // key off
  ym.run_to(now + 3000 * Ym2612::kMasterPerSample);
  ym.clear_samples();
  ym.run_to(now + 3200 * Ym2612::kMasterPerSample);
  CHECK(energy(ym) == 0); // the release (RR 15) ended
}

TEST_CASE("one sample every 144 chip clocks") {
  Ym2612 ym;
  ym.run_to(Ym2612::kMasterPerSample * 10);
  CHECK(ym.sample_count() == 10);
  ym.run_to(Ym2612::kMasterPerSample * 10 + 1000);
  CHECK(ym.sample_count() == 10);
  ym.run_to(Ym2612::kMasterPerSample * 11);
  CHECK(ym.sample_count() == 11);
}

TEST_CASE("timer A overflows after (1024 - N) samples") {
  Ym2612 ym;
  std::uint64_t now = 0;
  reg(ym, now, 0, 0x24, 1000 >> 2);
  reg(ym, now, 0, 0x25, 1000 & 3);
  reg(ym, now, 0, 0x27, 0x15);             // load A, enable A flag, reset A
  const std::uint64_t armed = now - kStep; // the data write loaded the timer
  const std::uint64_t period = (1024 - 1000) * 144ULL * 7ULL;
  CHECK((ym.status(armed + period - 7 * 4) & 1U) == 0);
  CHECK((ym.status(armed + period + 7 * 4) & 1U) == 1);
  CHECK((ym.status(armed + period + 7 * 4000) & 1U) == 1); // stays set
  now = armed + period + 7 * 4001;
  reg(ym, now, 0, 0x27, 0x15); // reset the flag, timer keeps loaded
  CHECK((ym.status(now + 1) & 1U) == 0);
}

TEST_CASE("timer B overflows after 16 * (256 - N) samples") {
  Ym2612 ym;
  std::uint64_t now = 0;
  reg(ym, now, 0, 0x26, 250);
  reg(ym, now, 0, 0x27, 0x2A); // load B, enable B flag, reset B
  const std::uint64_t armed = now - kStep;
  const std::uint64_t period = 6 * 16 * 144ULL * 7ULL;
  CHECK((ym.status(armed + period - 7 * 10) & 2U) == 0);
  CHECK((ym.status(armed + period + 7 * 10) & 2U) == 2);
}

TEST_CASE("two chips given the same accesses give the same samples") {
  Ym2612 a, b;
  std::uint64_t na = 0, nb = 0;
  program_voice(a, na);
  program_voice(b, nb);
  reg(a, na, 0, 0x28, 0xF0);
  reg(b, nb, 0, 0x28, 0xF0);
  a.run_to(na + 300 * Ym2612::kMasterPerSample);
  b.run_to(nb + 300 * Ym2612::kMasterPerSample);
  REQUIRE(a.samples().size() == b.samples().size());
  for (std::size_t i = 0; i < a.samples().size(); ++i) {
    CHECK(a.samples()[i].left == b.samples()[i].left);
    CHECK(a.samples()[i].right == b.samples()[i].right);
  }
}
