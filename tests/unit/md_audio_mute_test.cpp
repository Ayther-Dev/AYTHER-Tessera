// TP-107.5 (RF-3.7, RF-3.9): AUDIO_MUTE silences channels on the mix only. The
// state and the savestate are the same with any mask; the mask survives reset
// and restore; FM bits touch only the YM, PSG bits only the PSG.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace tessera::md;

namespace {
using Machine = std::unique_ptr<System>;

Machine run(std::uint32_t mask, unsigned frames) {
  auto system = std::make_unique<System>(
      tessera::testrom::fm_psg_sweep_rom(5), MachineConfig::for_region(Region::usa));
  system->reset();
  system->set_audio_mute(mask);
  for (unsigned i = 0; i < frames; ++i)
    system->run_frame();
  return system;
}

std::vector<std::int16_t> audio(const System &s) {
  return {s.audio().begin(), s.audio().end()};
}
std::vector<YmSample> ym(const System &s) {
  return {s.ym().samples().begin(), s.ym().samples().end()};
}
std::vector<std::int16_t> psg(const System &s) {
  return {s.psg().samples().begin(), s.psg().samples().end()};
}
bool equal(const std::vector<YmSample> &a, const std::vector<YmSample> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].left != b[i].left || a[i].right != b[i].right)
      return false;
  return true;
}
} // namespace

TEST_CASE("the mask never reaches the state or the savestate") {
  const auto open = run(0, 14);
  for (const std::uint32_t mask : {0x3FU, 0x3C0U, 0x3FFFFU, 0x0005U}) {
    const auto muted = run(mask, 14);
    CHECK(muted->save_state() == open->save_state());
    CHECK(muted->audio_writes().size() == open->audio_writes().size());
  }
}

TEST_CASE("muting silences the output and unmuting restores it") {
  const auto open = run(0, 14);
  const auto all = run(System::kAudioMuteBits, 14);
  CHECK(audio(*open) != audio(*all));
  // All muted: the output is flat (the chip's silent level), not the music.
  const auto flat = audio(*all);
  REQUIRE(flat.size() > 16);
  for (std::size_t i = 2; i < flat.size(); ++i)
    CHECK(flat[i] == flat[i % 2]);

  auto toggled = run(System::kAudioMuteBits, 10);
  toggled->set_audio_mute(0);
  auto reference = run(0, 10);
  toggled->run_frame();
  reference->run_frame();
  // Back to open, the frame is the unmuted one except for the mix tail of the
  // interval that straddled the change (the first output sample).
  const auto a = audio(*toggled), b = audio(*reference);
  REQUIRE(a.size() == b.size());
  for (std::size_t i = 4; i < a.size(); ++i)
    CHECK(a[i] == b[i]);
}

TEST_CASE("FM bits act on the YM only, PSG bits on the PSG only") {
  const auto open = run(0, 14);
  const auto fm = run(0x3F, 14);
  const auto tone = run(0x3C0, 14);
  CHECK(equal(ym(*tone), ym(*open)));
  CHECK(psg(*tone) != psg(*open));
  CHECK(psg(*fm) == psg(*open));
  CHECK_FALSE(equal(ym(*fm), ym(*open)));
  // PCM bits (10-17) are not channels of this console.
  const auto pcm = run(0x3FC00, 14);
  CHECK(audio(*pcm) == audio(*open));
  CHECK(pcm->audio_mute() == 0x3FC00U);
  // Bits above 18 are dropped.
  CHECK(run(0xFFFFFFFFU, 1)->audio_mute() == System::kAudioMuteBits);
}

TEST_CASE("each channel has its own bit") {
  const auto open = run(0, 14);
  // Played channels: FM 0 and 1, PSG tones 0 and 1 and the noise.
  for (const unsigned bit : {0U, 1U}) {
    const auto one = run(1U << bit, 14);
    CHECK_FALSE(equal(ym(*one), ym(*open)));
    CHECK(psg(*one) == psg(*open));
  }
  // An idle FM channel adds the same silence muted or not.
  for (const unsigned bit : {2U, 3U, 4U, 5U})
    CHECK(equal(ym(*run(1U << bit, 14)), ym(*open)));
  for (const unsigned bit : {6U, 7U, 9U}) {
    const auto one = run(1U << bit, 14);
    CHECK(equal(ym(*one), ym(*open)));
    CHECK(psg(*one) != psg(*open));
  }
  // The third tone is silent in this program (attenuation 15 at reset).
  CHECK(psg(*run(1U << 8, 14)) == psg(*open));
}

TEST_CASE("the mask survives reset and restore") {
  auto s = run(0x41, 6);
  const auto state = s->save_state();
  s->reset();
  CHECK(s->audio_mute() == 0x41U);
  auto b = run(0x41, 1);
  REQUIRE(b->load_state(state));
  CHECK(b->audio_mute() == 0x41U);
}
