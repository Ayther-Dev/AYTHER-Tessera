// SN76489-style PSG of the Mega Drive VDP (spec 008, TP-107.3). Own
// implementation from the public behavior of the chip: three square tone
// channels, one noise channel with a 16 bit shift register, 4 bit attenuation
// per channel in 2 dB steps. The chip clock is master/15; the counters step
// every 16 chip clocks, so one output sample is 240 master cycles.
#pragma once
#include "md/state_io.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

class Psg {
public:
  static constexpr std::uint32_t kMasterPerSample = 15 * 16;

  Psg() { reset(); }
  void reset() noexcept;
  /// Write of one byte to the chip (latch or data byte), at master cycle
  /// `master`; the chip is run up to that cycle first.
  void write(std::uint8_t value, std::uint64_t master);
  void run_to(std::uint64_t master);

  /// Mono samples at master/240 (signed 16 bit) since the last clear.
  std::span<const std::int16_t> samples() const noexcept { return out_; }
  /// The same samples as if nothing were muted (the savestate keeps these).
  std::span<const std::int16_t> raw_samples() const noexcept { return raw_; }
  void clear_samples() noexcept {
    out_.clear();
    raw_.clear();
  }
  std::uint64_t sample_count() const noexcept { return produced_; }

  // Chip state, for tests and the savestate.
  std::uint16_t tone_period(unsigned channel) const noexcept {
    return tone_[channel & 3U];
  }
  std::uint8_t attenuation(unsigned channel) const noexcept {
    return atten_[channel & 3U];
  }
  std::uint8_t noise_control() const noexcept { return noise_ctl_; }
  std::uint16_t lfsr() const noexcept { return lfsr_; }
  /// Output mute (TP-107.5): bits 0-2 the tone channels, bit 3 the noise. Not
  /// part of the chip state.
  void set_mute(std::uint8_t mask) noexcept { mute_ = mask & 0xFU; }
  std::uint8_t mute() const noexcept { return mute_; }
  void save(StateWriter &w) const;
  void load(StateReader &r);

private:
  void step() noexcept;
  std::int16_t mix(std::uint8_t mute) const noexcept;

  std::array<std::uint16_t, 3> tone_{}; ///< 10 bit periods
  std::array<std::uint16_t, 3> counter_{};
  std::array<std::uint8_t, 3> flip_{};  ///< tone output levels
  std::array<std::uint8_t, 4> atten_{}; ///< 0 loudest, 15 off
  std::uint8_t noise_ctl_ = 0;          ///< bit 2 white, bits 1-0 rate
  std::uint16_t noise_counter_ = 0;
  std::uint16_t lfsr_ = 0x8000;
  std::uint8_t noise_out_ = 0;
  std::uint8_t latch_ = 0; ///< channel (bits 2-1) and type (bit 0)
  std::uint64_t produced_ = 0;
  std::uint8_t mute_ = 0;
  std::vector<std::int16_t> out_;
  std::vector<std::int16_t> raw_;
};

} // namespace tessera::md
