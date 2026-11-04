// TP-101.2 (RF-2.3, RF-2.4): configuration, time constants and scheduler of the
// native Mega Drive machine. Constants come from plan 1.3.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/machine.hpp"
#include <doctest/doctest.h>

#include <algorithm>

using namespace tessera::md;

TEST_CASE("cycles per frame are 896040 (NTSC) and 1070460 (PAL)") {
  CHECK(kCyclesPerLine == 3420);
  CHECK(lines_per_frame(Standard::ntsc) == 262);
  CHECK(lines_per_frame(Standard::pal) == 313);
  CHECK(cycles_per_frame(Standard::ntsc) == 896040);
  CHECK(cycles_per_frame(Standard::pal) == 1070460);
}

TEST_CASE("master clock and component dividers") {
  CHECK(master_hz(Standard::ntsc) == 53693175);
  CHECK(master_hz(Standard::pal) == 53203424);
  CHECK(kDivider68k == 7);
  CHECK(kDividerZ80 == 15);
  CHECK(dot_divider(DisplayWidth::h40) == 8);
  CHECK(dot_divider(DisplayWidth::h32) == 10);
}

TEST_CASE("default configuration follows the region") {
  const auto usa = MachineConfig::for_region(Region::usa);
  CHECK(usa.standard == Standard::ntsc);
  CHECK(usa.region == Region::usa);
  CHECK(usa.pads[0] == PadType::three_button);
  CHECK(usa.pads[1] == PadType::three_button);
  CHECK_FALSE(usa.tmss);
  CHECK(usa.mapper == Mapper::none);
  CHECK(MachineConfig::for_region(Region::japan).standard == Standard::ntsc);
  CHECK(MachineConfig::for_region(Region::europe).standard == Standard::pal);
}

TEST_CASE("the scheduler counts lines and frames in master cycles") {
  Scheduler s(Standard::ntsc);
  CHECK(s.master() == 0);
  CHECK(s.line() == 0);
  s.advance(3419);
  CHECK(s.line() == 0);
  CHECK(s.line_cycle() == 3419);
  s.advance(1);
  CHECK(s.line() == 1);
  CHECK(s.line_cycle() == 0);
  CHECK(s.frame() == 0);
  s.advance(cycles_per_frame(Standard::ntsc) - 3420);
  CHECK(s.frame() == 1);
  CHECK(s.line() == 0);
  CHECK(s.frame_cycle() == 0);
}

TEST_CASE("component ticks do not drift: floor(total/divider) at any split") {
  Scheduler s(Standard::ntsc);
  std::uint64_t t68k = 0, tz80 = 0;
  // 7 frames of unequal slices; the remainders are carried between calls.
  const std::uint64_t total = 7 * cycles_per_frame(Standard::ntsc);
  std::uint64_t done = 0;
  std::uint64_t slice = 1;
  while (done < total) {
    const auto n = std::min<std::uint64_t>(slice, total - done);
    const auto ticks = s.advance(n);
    t68k += ticks.m68k;
    tz80 += ticks.z80;
    done += n;
    slice = slice * 3 % 977 + 1;
  }
  CHECK(done == total);
  CHECK(t68k == total / 7);
  CHECK(tz80 == total / 15);
  CHECK(s.frame() == 7);
}

TEST_CASE("one frame of 68000 and Z80 ticks") {
  Scheduler s(Standard::pal);
  const auto ticks = s.advance(cycles_per_frame(Standard::pal));
  CHECK(ticks.m68k == 1070460 / 7);
  CHECK(ticks.z80 == 1070460 / 15);
}
