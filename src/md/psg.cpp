#include "md/psg.hpp"

#include <cmath>

namespace tessera::md {

namespace {
// Amplitude of one channel by attenuation: 2 dB per step, 15 is silent.
const std::array<std::int16_t, 16> kVolume = [] {
  std::array<std::int16_t, 16> table{};
  for (unsigned i = 0; i < 15; ++i)
    table[i] = static_cast<std::int16_t>(
        std::lround(4095.0 * std::pow(10.0, -0.1 * static_cast<double>(i))));
  table[15] = 0;
  return table;
}();
} // namespace

void Psg::reset() noexcept {
  tone_ = {};
  counter_ = {};
  flip_ = {};
  atten_ = {15, 15, 15, 15};
  noise_ctl_ = 0;
  noise_counter_ = 0;
  lfsr_ = 0x8000;
  noise_out_ = 0;
  latch_ = 0;
  produced_ = 0;
  out_.clear();
}

void Psg::write(std::uint8_t value, std::uint64_t master) {
  run_to(master);
  if ((value & 0x80U) != 0) {
    latch_ = static_cast<std::uint8_t>((value >> 4) & 7U); // channel + type
  }
  const unsigned channel = (latch_ >> 1) & 3U;
  const bool attenuation = (latch_ & 1U) != 0;
  const bool latch_byte = (value & 0x80U) != 0;
  if (attenuation) {
    atten_[channel] = value & 0x0FU;
  } else if (channel == 3) {
    noise_ctl_ = value & 7U;
    lfsr_ = 0x8000; // any write to the noise register restarts the register
  } else if (latch_byte) {
    tone_[channel] = static_cast<std::uint16_t>((tone_[channel] & 0x3F0U) |
                                                (value & 0x0FU));
  } else {
    tone_[channel] = static_cast<std::uint16_t>(
        (tone_[channel] & 0x00FU) | ((value & 0x3FU) << 4));
  }
}

void Psg::step() noexcept {
  for (unsigned c = 0; c < 3; ++c) {
    if (counter_[c] > 0)
      --counter_[c];
    if (counter_[c] == 0) {
      counter_[c] = tone_[c] == 0 ? 1 : tone_[c];
      flip_[c] ^= 1U;
    }
  }
  if (noise_counter_ > 0)
    --noise_counter_;
  if (noise_counter_ == 0) {
    const unsigned rate = noise_ctl_ & 3U;
    noise_counter_ = rate == 3 ? (tone_[2] == 0 ? 1 : tone_[2])
                               : static_cast<std::uint16_t>(0x10U << rate);
    // The register shifts on every second step of the noise counter.
    noise_out_ ^= 1U;
    if (noise_out_ != 0) {
      const unsigned feedback =
          (noise_ctl_ & 4U) != 0
              ? ((lfsr_ ^ (lfsr_ >> 3)) & 1U) // white: taps 0 and 3
              : (lfsr_ & 1U);                 // periodic
      lfsr_ = static_cast<std::uint16_t>((lfsr_ >> 1) | (feedback << 15));
    }
  }
}

std::int16_t Psg::mix() const noexcept {
  int sum = 0;
  for (unsigned c = 0; c < 3; ++c)
    sum += flip_[c] != 0 ? kVolume[atten_[c]] : -kVolume[atten_[c]];
  sum += (lfsr_ & 1U) != 0 ? kVolume[atten_[3]] : -kVolume[atten_[3]];
  return static_cast<std::int16_t>(sum * 2);
}

void Psg::run_to(std::uint64_t master) {
  const std::uint64_t target = master / kMasterPerSample;
  while (produced_ < target) {
    step();
    out_.push_back(mix());
    ++produced_;
  }
}

} // namespace tessera::md
