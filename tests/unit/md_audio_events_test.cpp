// TP-107.6 (RF-3.7): the audio event transport. The queue is a bounded SPSC
// ring of 80 byte ayther_audio_event_v1 records; the System publishes a
// RAW_WRITE per sound chip write and a FRAME per frame while it is on.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/audio_events.hpp"
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <vector>

using namespace tessera::md;

namespace {
AudioEvent event(std::uint32_t reg) {
  AudioEvent e{};
  e.reg = reg;
  return e;
}
} // namespace

TEST_CASE("the queue keeps order, wraps and reports its stats") {
  AudioEventQueue q;
  auto s = q.stats(0);
  CHECK(s.struct_size == 32);
  CHECK(s.event_size == 80);
  CHECK(s.capacity == 8191);
  CHECK(s.pending == 0);

  std::vector<AudioEvent> out(8);
  for (std::uint32_t i = 0; i < 5; ++i)
    CHECK(q.push(event(i)));
  CHECK(q.stats(0).pending == 5);
  CHECK(q.stats(0).high_water_mark == 5);
  CHECK(q.poll(std::span(out).first(3)) == 3);
  CHECK(out[0].reg == 0);
  CHECK(out[2].reg == 2);
  CHECK(q.stats(0).pending == 2);
  CHECK(q.poll(out) == 2);
  CHECK(out[1].reg == 4);
  CHECK(q.poll(out) == 0);
  CHECK(q.stats(0).high_water_mark == 5);

  // many laps of the ring
  std::uint32_t next = 0, expected = 0;
  for (int lap = 0; lap < 5000; ++lap) {
    for (int k = 0; k < 7; ++k)
      CHECK(q.push(event(next++)));
    for (auto got = q.poll(out); got != 0; got = q.poll(out))
      for (std::size_t k = 0; k < got; ++k)
        CHECK(out[k].reg == expected++);
  }
  CHECK(expected == next);
}

TEST_CASE("a full queue drops and counts, never overwrites") {
  AudioEventQueue q;
  for (std::uint32_t i = 0; i < AudioEventQueue::kCapacity; ++i)
    REQUIRE(q.push(event(i)));
  CHECK(q.stats(0).pending == AudioEventQueue::kCapacity);
  CHECK_FALSE(q.push(event(99999)));
  CHECK_FALSE(q.push(event(99999)));
  CHECK(q.stats(0).dropped_events == 2);
  CHECK(q.stats(0).high_water_mark == AudioEventQueue::kCapacity);

  std::vector<AudioEvent> out(1);
  REQUIRE(q.poll(out) == 1);
  CHECK(out[0].reg == 0); // the oldest survived
  CHECK(q.push(event(7)));
}

TEST_CASE("the System publishes RAW_WRITE and FRAME only while subscribed") {
  using namespace tessera::testrom;
  System machine(fm_psg_sweep_rom(0), MachineConfig::for_region(Region::usa));
  machine.reset();
  machine.run_frame(); // not subscribed: nothing is published
  std::vector<AudioEvent> out(512);
  CHECK(machine.poll_audio_events(out) == 0);
  CHECK(machine.audio_transport_stats().flags == 0);

  machine.set_audio_events(true);
  CHECK(machine.audio_transport_stats().flags ==
        kAudioTransportObservationActive);
  machine.run_frame();
  const auto count = machine.poll_audio_events(out);
  REQUIRE(count >= 2);
  const auto writes = machine.audio_writes();
  REQUIRE(count == writes.size() + 1); // one RAW_WRITE per logged write + FRAME

  const std::uint64_t frame_cycles = cycles_per_frame(Standard::ntsc);
  for (std::size_t i = 0; i < writes.size(); ++i) {
    CHECK(out[i].type == static_cast<std::uint8_t>(AudioEventType::raw_write));
    CHECK(out[i].source == writes[i].chip);
    CHECK(out[i].reg == writes[i].addr);
    CHECK(out[i].data == writes[i].data);
    CHECK(out[i].t_cycles == writes[i].cycle);
    CHECK(out[i].t_frame == 1);
    CHECK(out[i].t_global == frame_cycles + writes[i].cycle);
    CHECK(out[i].channel == kAudioNoChannel);
    CHECK(out[i].schema == kAudioEventSchema);
    CHECK(out[i].group == 0);
  }
  const auto &last = out[count - 1];
  CHECK(last.type == static_cast<std::uint8_t>(AudioEventType::frame));
  CHECK(last.t_frame == 1);
  CHECK(last.t_cycles == frame_cycles);
  CHECK(last.t_global == 2 * frame_cycles);
}

TEST_CASE("the transport is not part of the savestate or of the run") {
  using namespace tessera::testrom;
  const auto rom = fm_psg_sweep_rom(0);
  System a(rom, MachineConfig::for_region(Region::usa));
  System b(rom, MachineConfig::for_region(Region::usa));
  a.reset();
  b.reset();
  b.set_audio_events(true);
  for (int i = 0; i < 6; ++i) {
    a.run_frame();
    b.run_frame();
  }
  CHECK(a.save_state() == b.save_state()); // observing changes nothing
}
