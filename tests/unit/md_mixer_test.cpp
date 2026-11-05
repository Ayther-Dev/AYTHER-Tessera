// TP-107.4 (RF-3.4): the mixer gives 48 kHz S16 stereo from the YM (1008
// master cycles per sample) and PSG (240) streams, with a stable count per
// frame and no seam between frames.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/mixer.hpp"
#include <doctest/doctest.h>

#include <cmath>

using namespace tessera::md;

namespace {
struct Feed {
  std::vector<YmSample> ym;
  std::vector<std::int16_t> psg;
};
/// Source samples of the given frames, from a pure function of the sample
/// index (so any frame split sees the same stream). A frame holds the samples
/// that end inside it, as the chips produce them.
Feed source(Standard s, unsigned first_frame, unsigned frames) {
  Feed f;
  const std::uint64_t cpf = cycles_per_frame(s);
  const auto from = first_frame * cpf, to = (first_frame + frames) * cpf;
  for (auto i = from / 1008; i < to / 1008; ++i)
    f.ym.push_back({static_cast<std::int32_t>((i * 37) % 2001) - 1000,
                    static_cast<std::int32_t>((i * 91) % 1501) - 750});
  for (auto i = from / 240; i < to / 240; ++i)
    f.psg.push_back(static_cast<std::int16_t>((i % 7) * 100));
  return f;
}
} // namespace

TEST_CASE("48 kHz stereo per frame, fraction carried across frames") {
  for (const auto s : {Standard::ntsc, Standard::pal}) {
    Mixer m(s);
    std::uint64_t total = 0;
    const unsigned frames = 600;
    const double hz = master_hz(s), per = cycles_per_frame(s);
    for (unsigned f = 0; f < frames; ++f) {
      const auto feed = source(s, f, 1);
      m.push(feed.ym, feed.psg);
      CHECK(m.output().size() % 2 == 0);
      const auto n = static_cast<double>(m.output().size() / 2);
      // 48000 * cycles_per_frame / master_hz: 800.x (NTSC), 960.x (PAL).
      const auto expect = 48000.0 * per / hz;
      CHECK(std::abs(n - expect) <= 2.0);
      total += m.output().size() / 2;
    }
    CHECK(total == m.sample_count());
    CHECK(std::abs(static_cast<double>(total) - 48000.0 * per * frames / hz) <=
          3.0);
  }
}

TEST_CASE("the output does not depend on how the stream is split into frames") {
  const auto s = Standard::ntsc;
  Mixer whole(s), split(s);
  const auto all = source(s, 0, 30);
  whole.push(all.ym, all.psg);
  const std::vector<std::int16_t> a(whole.output().begin(),
                                    whole.output().end());
  std::vector<std::int16_t> b;
  for (unsigned f = 0; f < 30; ++f) {
    const auto feed = source(s, f, 1);
    split.push(feed.ym, feed.psg);
    b.insert(b.end(), split.output().begin(), split.output().end());
  }
  CHECK(a == b);
}

TEST_CASE("a constant input stays constant and silence is zero") {
  Mixer m(Standard::pal);
  Feed f;
  f.ym.assign(2000, YmSample{1000, -500});
  f.psg.assign(9000, 200);
  m.push(f.ym, f.psg);
  REQUIRE(m.output().size() > 100);
  for (std::size_t i = 0; i + 1 < m.output().size(); i += 2) {
    CHECK(m.output()[i] == 1200);
    CHECK(m.output()[i + 1] == -300);
  }
  Mixer z(Standard::ntsc);
  Feed zero;
  zero.ym.assign(500, YmSample{});
  zero.psg.assign(2500, 0);
  z.push(zero.ym, zero.psg);
  REQUIRE(!z.output().empty());
  for (const auto v : z.output())
    CHECK(v == 0);
}

TEST_CASE("the sum saturates at 16 bits") {
  Feed f;
  f.ym.assign(300, YmSample{30000, -30000});
  f.psg.assign(1500, 20000);
  Mixer m(Standard::ntsc);
  m.push(f.ym, f.psg);
  REQUIRE(!m.output().empty());
  CHECK(m.output()[0] == 32767);
  CHECK(m.output()[1] == -10000);
  f.ym.assign(300, YmSample{-30000, 0});
  f.psg.assign(1500, -20000);
  Mixer n(Standard::ntsc);
  n.push(f.ym, f.psg);
  REQUIRE(!n.output().empty());
  CHECK(n.output()[0] == -32768);
}

TEST_CASE("save and load keep the stream going") {
  const auto s = Standard::ntsc;
  Mixer a(s), b(s);
  for (unsigned f = 0; f < 5; ++f) {
    const auto feed = source(s, f, 1);
    a.push(feed.ym, feed.psg);
  }
  std::vector<std::uint8_t> bytes;
  StateWriter w(bytes);
  a.save(w);
  StateReader r(bytes);
  b.load(r);
  CHECK(r.finished());
  for (unsigned f = 5; f < 9; ++f) {
    const auto feed = source(s, f, 1);
    a.push(feed.ym, feed.psg);
    b.push(feed.ym, feed.psg);
    CHECK(std::vector<std::int16_t>(a.output().begin(), a.output().end()) ==
          std::vector<std::int16_t>(b.output().begin(), b.output().end()));
  }
}
