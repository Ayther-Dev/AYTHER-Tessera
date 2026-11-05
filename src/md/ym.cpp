#include "md/ym.hpp"

#include "ymfm_opn.h"

#include <array>
#include <memory>
#include <vector>

namespace tessera::md {

namespace {
constexpr std::int64_t kNoDeadline = -1;
} // namespace

/// ymfm::ym2612 with a mute mask on its output (TP-107.5). The chip state is
/// untouched; only what is summed into the sample changes. With nothing muted
/// the base `generate` runs.
class MutableChip final : public ymfm::ym2612 {
public:
  using ymfm::ym2612::ym2612;
  std::uint8_t mute = 0; ///< bit n: FM channel n (5 is the DAC when enabled)

  /// One sample: `muted` with the mask applied, `raw` as if nothing were muted.
  void generate_muted(output_data *muted, output_data *raw) {
    if (mute == 0) {
      generate(muted, 1);
      *raw = *muted;
      return;
    }
    m_fm.clock(fm_engine::ALL_CHANNELS);
    *muted = sum(mute);
    *raw = sum(0);
  }

private:
  /// Same sum as ymfm's `generate` (9-bit channels with the DAC discontinuity),
  /// a muted channel adding the discontinuity of silence.
  output_data sum(std::uint8_t mask) {
    const auto disc = [](std::int32_t v) { return v < 0 ? v - 3 : v + 4; };
    output_data out;
    out.clear();
    output_data temp;
    const int last_fm_channel = m_dac_enable ? 5 : 6;
    for (int chan = 0; chan < last_fm_channel; ++chan) {
      temp.clear();
      if ((mask >> chan & 1U) == 0)
        m_fm.output(temp, 5, 256, 1U << chan);
      out.data[0] += disc(temp.data[0]);
      out.data[1] += disc(temp.data[1]);
    }
    if (m_dac_enable) {
      const std::int32_t dac = (mask >> 5 & 1U) != 0
                                   ? disc(0)
                                   : disc(std::int16_t(m_dac_data << 7) >> 7);
      out.data[0] += m_fm.regs().ch_output_0(0x102) ? dac : disc(0);
      out.data[1] += m_fm.regs().ch_output_1(0x102) ? dac : disc(0);
    }
    out.data[0] = (out.data[0] * 128) * 64 / (6 * 65);
    out.data[1] = (out.data[1] * 128) * 64 / (6 * 65);
    return out;
  }
};

struct Ym2612::Impl final : ymfm::ymfm_interface {
  Impl() : chip(*this) { chip.reset(); }

  void reset_state() {
    // ymfm's reset leaves some hidden state (counters, caches) from the run
    // before; a hard reset builds the chip again, so that it equals a new one.
    const auto mute = chip.mute;
    std::destroy_at(&chip);
    std::construct_at(&chip, *this);
    chip.mute = mute;
    chip.reset();
    clock = 0;
    produced = 0;
    deadline = {kNoDeadline, kNoDeadline};
    out.clear();
    raw.clear();
  }

  // ymfm_interface: timers are armed in chip clocks from "now".
  void ymfm_set_timer(std::uint32_t tnum, std::int32_t duration) override {
    deadline[tnum] = duration < 0 ? kNoDeadline
                                  : static_cast<std::int64_t>(clock) + duration;
  }

  void run_to(std::uint64_t master) {
    const std::uint64_t target = master / kMasterPerClock;
    while (clock < target) {
      std::uint64_t next = target;
      const std::uint64_t boundary = (produced + 1) * kClocksPerSample;
      if (boundary < next)
        next = boundary;
      for (const auto d : deadline)
        if (d != kNoDeadline && static_cast<std::uint64_t>(d) > clock &&
            static_cast<std::uint64_t>(d) < next)
          next = static_cast<std::uint64_t>(d);
      clock = next;
      for (std::uint32_t t = 0; t < 2; ++t)
        if (deadline[t] != kNoDeadline &&
            static_cast<std::uint64_t>(deadline[t]) <= clock) {
          deadline[t] = kNoDeadline;
          m_engine->engine_timer_expired(t);
        }
      if (clock == boundary) {
        ymfm::ym2612::output_data data, raw_data;
        chip.generate_muted(&data, &raw_data);
        out.push_back({data.data[0], data.data[1]});
        raw.push_back({raw_data.data[0], raw_data.data[1]});
        ++produced;
      }
    }
  }

  MutableChip chip;
  std::uint64_t clock = 0;    ///< chip clocks run so far
  std::uint64_t produced = 0; ///< samples generated so far
  std::array<std::int64_t, 2> deadline{kNoDeadline, kNoDeadline};
  std::vector<YmSample> out;
  std::vector<YmSample> raw; ///< the same samples with no mute
};

Ym2612::Ym2612() : impl_(std::make_unique<Impl>()) {}
Ym2612::~Ym2612() = default;
Ym2612::Ym2612(Ym2612 &&) noexcept = default;
Ym2612 &Ym2612::operator=(Ym2612 &&) noexcept = default;

void Ym2612::reset() { impl_->reset_state(); }

void Ym2612::soft_reset() {
  impl_->chip.reset();
  impl_->deadline = {kNoDeadline, kNoDeadline};
}

std::span<const YmSample> Ym2612::raw_samples() const noexcept {
  return impl_->raw;
}

void Ym2612::run_to(std::uint64_t master) { impl_->run_to(master); }

void Ym2612::write(unsigned port, std::uint8_t value, std::uint64_t master) {
  impl_->run_to(master);
  impl_->chip.write(port & 3U, value);
}

std::uint8_t Ym2612::status(std::uint64_t master) {
  impl_->run_to(master);
  return impl_->chip.read_status();
}

std::span<const YmSample> Ym2612::samples() const noexcept {
  return impl_->out;
}
void Ym2612::clear_samples() noexcept {
  impl_->out.clear();
  impl_->raw.clear();
}
std::uint64_t Ym2612::sample_count() const noexcept { return impl_->produced; }

void Ym2612::set_mute(std::uint8_t mask) noexcept {
  impl_->chip.mute = mask & 0x3FU;
}
std::uint8_t Ym2612::mute() const noexcept { return impl_->chip.mute; }

void Ym2612::save(StateWriter &w) const {
  auto &impl = *impl_;
  std::vector<std::uint8_t> chip;
  ymfm::ymfm_saved_state state(chip, true);
  impl.chip.save_restore(state);
  w.block(chip);
  w.u64(impl.clock);
  w.u64(impl.produced);
  for (const auto d : impl.deadline)
    w.i64(d);
}

void Ym2612::load(StateReader &r) {
  auto &impl = *impl_;
  const auto block = r.block(1U << 16);
  std::vector<std::uint8_t> chip(block.begin(), block.end());
  if (r.ok()) {
    ymfm::ymfm_saved_state state(chip, false);
    impl.chip.save_restore(state);
  }
  impl.clock = r.u64();
  impl.produced = r.u64();
  for (auto &d : impl.deadline)
    d = r.i64();
  impl.out.clear();
  impl.raw.clear();
}

} // namespace tessera::md
