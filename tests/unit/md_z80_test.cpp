// TP-103.1 (RF-2.1): floooh/chips z80.h behind the native Mega Drive adapter.
// Synthetic programs run on the Z80: the final memory and registers are the
// expected ones; wait states and the INT pin are exercised too.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/z80.hpp"
#include <doctest/doctest.h>

#include <array>
#include <initializer_list>

using namespace tessera::md;

namespace {
struct FlatBus : Z80Bus {
  std::array<std::uint8_t, 0x10000> mem{};
  unsigned read_wait = 0;
  unsigned reads = 0, writes = 0;
  std::uint8_t read(std::uint16_t address, unsigned &wait) override {
    ++reads;
    wait = read_wait;
    return mem[address];
  }
  void write(std::uint16_t address, std::uint8_t value,
             unsigned &wait) override {
    ++writes;
    wait = 0;
    mem[address] = value;
  }
  void load(std::uint16_t at, std::initializer_list<std::uint8_t> bytes) {
    for (const auto b : bytes)
      mem[at++] = b;
  }
};

// Sum loop: stores 10, 19, 27, ... 55 at $0100..$0109, then HALT.
void load_sum_loop(FlatBus &bus) {
  bus.load(0x0000, {0x21, 0x00, 0x01, // ld hl,$0100
                    0x06, 0x0A,       // ld b,10
                    0xAF,             // xor a
                    0x80,             // loop: add a,b
                    0x77,             // ld (hl),a
                    0x23,             // inc hl
                    0x10, 0xFB,       // djnz loop
                    0x76});           // halt
}

unsigned run_until_halt(Z80 &cpu, unsigned limit = 4000) {
  unsigned ticks = 0;
  while (!cpu.halted() && ticks < limit) {
    cpu.tick();
    ++ticks;
  }
  return ticks;
}
} // namespace

TEST_CASE("reset starts at address 0 with every register at 0") {
  FlatBus bus;
  Z80 cpu(bus);
  CHECK(cpu.pc() == 0);
  CHECK(cpu.sp() == 0); // the Mega Drive Z80 resets every register to 0
  CHECK(cpu.im() == 0);
  CHECK_FALSE(cpu.iff1());
}

TEST_CASE("a synthetic program leaves the expected memory") {
  FlatBus bus;
  load_sum_loop(bus);
  Z80 cpu(bus);
  const unsigned ticks = run_until_halt(cpu);
  REQUIRE(cpu.halted());
  const std::array<std::uint8_t, 10> expected = {10, 19, 27, 34, 40,
                                                 45, 49, 52, 54, 55};
  for (std::size_t i = 0; i < expected.size(); ++i)
    CHECK(bus.mem[0x0100 + i] == expected[i]);
  CHECK(cpu.hl() == 0x010A);
  CHECK((cpu.af() >> 8) == 55);
  // 21 T-states of set-up, 10 x 17 of body, 9 x 13 + 8 of DJNZ, then the HALT.
  CHECK(ticks >= 316);
  CHECK(ticks <= 324);
  CHECK(bus.writes == 10);
}

TEST_CASE("wait states from the bus stretch every access") {
  FlatBus fast, slow;
  load_sum_loop(fast);
  load_sum_loop(slow);
  slow.read_wait = 2;
  Z80 a(fast), b(slow);
  const unsigned fast_ticks = run_until_halt(a);
  const unsigned slow_ticks = run_until_halt(b);
  REQUIRE(a.halted());
  REQUIRE(b.halted());
  CHECK(slow.mem[0x0109] == 55); // the result does not change
  CHECK(slow_ticks > fast_ticks + 2U * fast.reads / 2U);
}

TEST_CASE("INT in mode 1 jumps to $0038 once interrupts are enabled") {
  FlatBus bus;
  bus.load(0x0000, {0xED, 0x56,       // im 1
                    0xFB,             // ei
                    0x76});           // halt
  bus.load(0x0038, {0x3E, 0x77,       // ld a,$77
                    0x32, 0x00, 0x02, // ld ($0200),a
                    0x76});           // halt
  Z80 cpu(bus);
  for (unsigned i = 0; i < 40; ++i)
    cpu.tick();
  CHECK(bus.mem[0x0200] == 0);
  cpu.set_int(true);
  for (unsigned i = 0; i < 60; ++i) {
    cpu.tick();
    if (i == 20)
      cpu.set_int(false);
  }
  CHECK(bus.mem[0x0200] == 0x77);
  CHECK(cpu.im() == 1);
}

TEST_CASE("INT is ignored while interrupts are disabled") {
  FlatBus bus;
  bus.load(0x0000, {0xF3, 0x76}); // di ; halt
  bus.load(0x0038, {0x3E, 0x77, 0x32, 0x00, 0x02, 0x76});
  Z80 cpu(bus);
  cpu.set_int(true);
  for (unsigned i = 0; i < 100; ++i)
    cpu.tick();
  CHECK(bus.mem[0x0200] == 0);
}

TEST_CASE("reset returns the CPU to address 0") {
  FlatBus bus;
  load_sum_loop(bus);
  Z80 cpu(bus);
  run_until_halt(cpu);
  cpu.reset();
  CHECK(cpu.pc() == 0);
  CHECK_FALSE(cpu.halted());
}
