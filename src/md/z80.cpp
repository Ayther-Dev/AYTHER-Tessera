#include "md/z80.hpp"

namespace tessera::md {
namespace {
constexpr std::uint64_t kDataShift = 16;
constexpr std::uint64_t kDataMask = 0xFFULL << kDataShift;
} // namespace

Z80::Z80(Z80Bus &bus) : bus_(bus) { reset(); }

void Z80::reset() {
  pins_ = tessera_z80_reset(&cpu_);
  wait_ = 0;
}

void Z80::tick() {
  ++cycles_;
  // Wait states: the chip samples its WAIT pin before it issues an access, so
  // a wait asked by the access itself cannot be signalled in time. The CPU is
  // frozen instead for as many T-states as the bus asked.
  if (wait_ != 0) {
    --wait_;
    return;
  }
  if (int_)
    pins_ |= TZ80_INT;
  else
    pins_ &= ~TZ80_INT;
  pins_ = tessera_z80_tick(&cpu_, pins_);
  const auto address = static_cast<std::uint16_t>(pins_ & 0xFFFFU);
  if ((pins_ & TZ80_MREQ) != 0) {
    unsigned wait = 0;
    if ((pins_ & TZ80_RD) != 0) {
      const std::uint8_t value = bus_.read(address, wait);
      pins_ = (pins_ & ~kDataMask) | (std::uint64_t{value} << kDataShift);
    } else if ((pins_ & TZ80_WR) != 0) {
      bus_.write(address, static_cast<std::uint8_t>(pins_ >> kDataShift), wait);
    }
    wait_ = wait;
  } else if ((pins_ & TZ80_IORQ) != 0) {
    // Interrupt acknowledge and port accesses: the Mega Drive has no I/O space
    // on the Z80 side; the data bus floats high (RST 38h for IM 1 / IM 0).
    if ((pins_ & TZ80_RD) != 0 || (pins_ & (1ULL << 24)) != 0)
      pins_ |= kDataMask;
  }
}

} // namespace tessera::md
