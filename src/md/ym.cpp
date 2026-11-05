#include "md/ym.hpp"

#include "ymfm_opn.h"

#include <array>
#include <vector>

namespace tessera::md {

namespace {
constexpr std::int64_t kNoDeadline = -1;
} // namespace

struct Ym2612::Impl final : ymfm::ymfm_interface {
  Impl() : chip(*this) { chip.reset(); }

  void reset_state() {
    chip.reset();
    clock = 0;
    produced = 0;
    deadline = {kNoDeadline, kNoDeadline};
    out.clear();
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
        ymfm::ym2612::output_data data;
        chip.generate(&data, 1);
        out.push_back({data.data[0], data.data[1]});
        ++produced;
      }
    }
  }

  ymfm::ym2612 chip;
  std::uint64_t clock = 0;    ///< chip clocks run so far
  std::uint64_t produced = 0; ///< samples generated so far
  std::array<std::int64_t, 2> deadline{kNoDeadline, kNoDeadline};
  std::vector<YmSample> out;
};

Ym2612::Ym2612() : impl_(std::make_unique<Impl>()) {}
Ym2612::~Ym2612() = default;
Ym2612::Ym2612(Ym2612 &&) noexcept = default;
Ym2612 &Ym2612::operator=(Ym2612 &&) noexcept = default;

void Ym2612::reset() { impl_->reset_state(); }

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
void Ym2612::clear_samples() noexcept { impl_->out.clear(); }
std::uint64_t Ym2612::sample_count() const noexcept { return impl_->produced; }

} // namespace tessera::md
