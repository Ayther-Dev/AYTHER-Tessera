#include "sms/ym2413.hpp"

#include "ymfm_opl.h"

namespace tessera::sms {

class MutableYm2413 final : public ymfm::ym2413 {
public:
  using ymfm::ym2413::ym2413;

  void generate_with_mute(output_data *out, std::uint16_t mute_mask,
                          bool rhythm) {
    if (mute_mask == 0) {
      generate(out, 1);
      return;
    }
    m_fm.clock(fm_engine::ALL_CHANNELS);
    out->clear();
    output_data temp;
    const int mel_chans = rhythm ? 6 : 9;
    for (int ch = 0; ch < mel_chans; ++ch) {
      if (((mute_mask >> ch) & 1U) != 0)
        continue;
      temp.clear();
      m_fm.output(temp, 1, 32767, 1U << ch);
      out->data[0] += temp.data[0];
      out->data[1] += temp.data[1];
    }
    if (rhythm) {
      for (int r = 0; r < 5; ++r) {
        if (((mute_mask >> (9 + r)) & 1U) != 0)
          continue;
        temp.clear();
        m_fm.output(temp, 1, 32767, 1U << (6 + r));
        out->data[0] += temp.data[0];
        out->data[1] += temp.data[1];
      }
    }
  }
};

struct Ym2413::Impl final : public ymfm::ymfm_interface {
  Impl() : chip(*this) { chip.reset(); }
  MutableYm2413 chip;
};

Ym2413::Ym2413() : impl_(std::make_unique<Impl>()) {}
Ym2413::~Ym2413() = default;
Ym2413::Ym2413(Ym2413 &&) noexcept = default;
Ym2413 &Ym2413::operator=(Ym2413 &&) noexcept = default;

void Ym2413::reset() noexcept {
  regs_.fill(0);
  addr_ = 0;
  detect_ = 0;
  if (impl_)
    impl_->chip.reset();
}

void Ym2413::write_addr(std::uint8_t addr) noexcept {
  addr_ = addr;
  if (impl_)
    impl_->chip.write_address(addr);
}

void Ym2413::write_data(std::uint8_t value) noexcept {
  if (addr_ < regs_.size())
    regs_[addr_] = value;
  if (impl_)
    impl_->chip.write_data(value);
}

void Ym2413::step_sample(std::int32_t &left, std::int32_t &right) noexcept {
  if (!impl_)
    return;
  MutableYm2413::output_data out;
  const bool rhythm = (regs_[0x0E] & 0x20U) != 0;
  impl_->chip.generate_with_mute(&out, mute_, rhythm);
  left += out.data[0];
  right += out.data[1];
}

void Ym2413::save(md::StateWriter &w) const {
  w.bytes(regs_);
  w.u8(addr_);
  w.u8(detect_);
}

void Ym2413::load(md::StateReader &r) {
  r.bytes(regs_);
  addr_ = r.u8();
  detect_ = r.u8();
  if (impl_) {
    impl_->chip.reset();
    for (std::uint8_t i = 0; i < regs_.size(); ++i) {
      impl_->chip.write_address(i);
      impl_->chip.write_data(regs_[i]);
    }
    impl_->chip.write_address(addr_);
  }
}

} // namespace tessera::sms
