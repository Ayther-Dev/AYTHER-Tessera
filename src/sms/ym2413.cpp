#include "sms/ym2413.hpp"

#include <cmath>

namespace tessera::sms {

void Ym2413::reset() noexcept {
  regs_.fill(0);
  phase_.fill(0);
  addr_ = 0;
  detect_ = 0;
}

void Ym2413::write_data(std::uint8_t value) noexcept {
  if (addr_ < regs_.size())
    regs_[addr_] = value;
}

void Ym2413::step_sample(std::int32_t &left, std::int32_t &right) noexcept {
  std::int32_t sum = 0;
  const bool rhythm_mode = (regs_[0x0E] & 0x20U) != 0;
  const int mel_channels = rhythm_mode ? 6 : 9;

  for (int ch = 0; ch < mel_channels; ++ch) {
    const std::uint8_t r20 = regs_[0x20 + ch];
    const bool key_on = (r20 & 0x10U) != 0;
    if (!key_on)
      continue;
    const std::uint16_t fnum = static_cast<std::uint16_t>(
        regs_[0x10 + ch] | ((std::uint16_t{r20} & 0x01U) << 8));
    const std::uint8_t block = static_cast<std::uint8_t>((r20 >> 1) & 0x07U);
    const std::uint8_t vol =
        static_cast<std::uint8_t>(regs_[0x30 + ch] & 0x0FU);
    const std::uint32_t inc = (static_cast<std::uint32_t>(fnum) << block) >> 1;
    phase_[ch] += inc;
    if (((mute_ >> ch) & 1U) == 0) {
      const std::int32_t amp = static_cast<std::int32_t>(15 - vol) * 64;
      const std::int32_t wave = ((phase_[ch] & 0x8000U) != 0) ? amp : -amp;
      sum += wave;
    }
  }

  left += sum;
  right += sum;
}

void Ym2413::save(md::StateWriter &w) const {
  w.bytes(regs_);
  w.array(phase_);
  w.u8(addr_);
  w.u8(detect_);
}

void Ym2413::load(md::StateReader &r) {
  r.bytes(regs_);
  r.array(phase_);
  addr_ = r.u8();
  detect_ = r.u8();
}

} // namespace tessera::sms
