// YM2612 of the native Mega Drive core (spec 008, TP-107.1). ymfm (BSD-3)
// emulates the chip; this class gives it the time base of the machine: the
// chip clock is master/7 and one sample is 144 chip clocks (1008 master
// cycles). Writes and reads state the master cycle they happen at; the chip is
// run up to that cycle first, so timers and the sample stream depend only on
// the sequence of (cycle, access) pairs.
#pragma once
#include "md/state_io.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace tessera::md {

struct YmSample {
  std::int32_t left = 0;
  std::int32_t right = 0;
};

class Ym2612 {
public:
  /// Master cycles per chip clock and per sample.
  static constexpr std::uint32_t kMasterPerClock = 7;
  static constexpr std::uint32_t kClocksPerSample = 144;
  static constexpr std::uint32_t kMasterPerSample =
      kMasterPerClock * kClocksPerSample;

  Ym2612();
  ~Ym2612();
  Ym2612(const Ym2612 &) = delete;
  Ym2612 &operator=(const Ym2612 &) = delete;
  Ym2612(Ym2612 &&) noexcept;
  Ym2612 &operator=(Ym2612 &&) noexcept;

  /// Power-on state; the clock goes back to master cycle 0.
  void reset();
  /// Runs the chip up to `master` (timers fire, samples are generated).
  void run_to(std::uint64_t master);
  /// Write to port 0-3 ($A04000-3): address/data of part 1, address/data of
  /// part 2.
  void write(unsigned port, std::uint8_t value, std::uint64_t master);
  /// Status register: timer A (bit 0) and B (bit 1) overflow flags. The busy
  /// bit belongs to the machine (it is measured on the bus, not in the chip).
  std::uint8_t status(std::uint64_t master);

  /// Samples generated since the last `clear_samples` (native chip rate).
  std::span<const YmSample> samples() const noexcept;
  void clear_samples() noexcept;
  /// Number of samples generated since reset.
  std::uint64_t sample_count() const noexcept;

  /// Savestate: the chip (ymfm save_restore), the clock and the timers.
  void save(StateWriter &w) const;
  void load(StateReader &r);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace tessera::md
