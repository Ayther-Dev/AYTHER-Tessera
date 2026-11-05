// Audio mixer of the native Mega Drive (spec 008, TP-107.4): the YM2612
// (one stereo sample per 1008 master cycles) and the PSG (one mono sample per
// 240) are integrated over exact 48 kHz output intervals (box filter) and
// summed into interleaved signed 16 bit stereo. Interval boundaries are
// rational (k * master_hz / 48000) and kept in integers, so the output depends
// only on the source samples and carries no drift between frames: NTSC and PAL
// give 48000/fps samples per frame on average, with the fraction carried across
// frames.
#pragma once
#include "md/machine.hpp"
#include "md/psg.hpp"
#include "md/state_io.hpp"
#include "md/ym.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

class Mixer {
public:
  static constexpr std::uint32_t kOutputRate = 48000;

  explicit Mixer(Standard standard = Standard::ntsc) noexcept
      : master_(master_hz(standard)) {}

  void reset() noexcept {
    ym_.clear();
    psg_.clear();
    ym_raw_.clear();
    psg_raw_.clear();
    ym_base_ = psg_base_ = 0;
    next_ = 0;
    out_.clear();
  }

  /// Appends the samples a frame produced and renders every output interval
  /// they complete. `output()` holds only what this call produced.
  void push(std::span<const YmSample> ym, std::span<const std::int16_t> psg) {
    push(ym, psg, ym, psg);
  }
  /// `*_raw` are the same samples with no mute applied; they are what the
  /// savestate keeps, so that the mute never shows in it.
  void push(std::span<const YmSample> ym, std::span<const std::int16_t> psg,
            std::span<const YmSample> ym_raw,
            std::span<const std::int16_t> psg_raw) {
    out_.clear();
    ym_.insert(ym_.end(), ym.begin(), ym.end());
    psg_.insert(psg_.end(), psg.begin(), psg.end());
    ym_raw_.insert(ym_raw_.end(), ym_raw.begin(), ym_raw.end());
    psg_raw_.insert(psg_raw_.end(), psg_raw.begin(), psg_raw.end());
    const std::uint64_t ym_end = (ym_base_ + ym_.size()) * kYmLen;
    const std::uint64_t psg_end = (psg_base_ + psg_.size()) * kPsgLen;
    const std::uint64_t covered = std::min(ym_end, psg_end);
    while ((next_ + 1) * master_ <= covered) {
      const auto t0 = next_ * master_;
      const auto t1 = t0 + master_;
      std::int64_t left = 0, right = 0;
      integrate_ym(t0, t1, left, right);
      const std::int64_t mono = integrate_psg(t0, t1);
      out_.push_back(clamp16(round_div(left) + round_div(mono)));
      out_.push_back(clamp16(round_div(right) + round_div(mono)));
      ++next_;
    }
    trim(next_ * master_);
  }

  /// Interleaved L/R samples rendered by the last `push`.
  std::span<const std::int16_t> output() const noexcept { return out_; }
  /// Output samples rendered since reset.
  std::uint64_t sample_count() const noexcept { return next_; }

  void save(StateWriter &w) const {
    w.u64(ym_base_);
    w.u64(psg_base_);
    w.u64(next_);
    w.u32(static_cast<std::uint32_t>(ym_raw_.size()));
    for (const auto &s : ym_raw_) {
      w.i32(s.left);
      w.i32(s.right);
    }
    w.u32(static_cast<std::uint32_t>(psg_raw_.size()));
    for (const auto s : psg_raw_)
      w.u16(static_cast<std::uint16_t>(s));
  }
  void load(StateReader &r) {
    ym_base_ = r.u64();
    psg_base_ = r.u64();
    next_ = r.u64();
    ym_raw_.clear();
    psg_raw_.clear();
    out_.clear();
    const auto ny = r.u32();
    if (ny > kMaxPending) {
      r.fail();
      return;
    }
    for (std::uint32_t i = 0; i < ny && r.ok(); ++i) {
      YmSample s;
      s.left = r.i32();
      s.right = r.i32();
      ym_raw_.push_back(s);
    }
    const auto np = r.u32();
    if (np > kMaxPending) {
      r.fail();
      return;
    }
    for (std::uint32_t i = 0; i < np && r.ok(); ++i)
      psg_raw_.push_back(static_cast<std::int16_t>(r.u16()));
    // The tail restored is the unmuted one; the next frames follow the mute.
    ym_ = ym_raw_;
    psg_ = psg_raw_;
  }

private:
  static constexpr std::uint32_t kMaxPending = 1U << 16;
  /// Length of one source sample in units of 1/48000 master cycle.
  static constexpr std::uint64_t kYmLen =
      std::uint64_t{Ym2612::kMasterPerSample} * kOutputRate;
  static constexpr std::uint64_t kPsgLen =
      std::uint64_t{Psg::kMasterPerSample} * kOutputRate;

  std::int64_t round_div(std::int64_t sum) const noexcept {
    const auto half = static_cast<std::int64_t>(master_ / 2);
    return (sum >= 0 ? sum + half : sum - half) /
           static_cast<std::int64_t>(master_);
  }
  static std::int16_t clamp16(std::int64_t v) noexcept {
    return static_cast<std::int16_t>(
        std::clamp<std::int64_t>(v, -32768, 32767));
  }
  /// Overlap of source sample `index` (length `len`) with [t0, t1).
  static std::int64_t overlap(std::uint64_t index, std::uint64_t len,
                              std::uint64_t t0, std::uint64_t t1) noexcept {
    const auto a = std::max(index * len, t0);
    const auto b = std::min((index + 1) * len, t1);
    return b > a ? static_cast<std::int64_t>(b - a) : 0;
  }
  void integrate_ym(std::uint64_t t0, std::uint64_t t1, std::int64_t &left,
                    std::int64_t &right) const noexcept {
    for (auto i = std::max(t0 / kYmLen, ym_base_); i * kYmLen < t1; ++i) {
      const auto &s = ym_[i - ym_base_];
      const auto o = overlap(i, kYmLen, t0, t1);
      left += std::int64_t{s.left} * o;
      right += std::int64_t{s.right} * o;
    }
  }
  std::int64_t integrate_psg(std::uint64_t t0,
                             std::uint64_t t1) const noexcept {
    std::int64_t sum = 0;
    for (auto i = std::max(t0 / kPsgLen, psg_base_); i * kPsgLen < t1; ++i)
      sum += std::int64_t{psg_[i - psg_base_]} * overlap(i, kPsgLen, t0, t1);
    return sum;
  }
  /// Drops the source samples that end at or before `t`.
  void trim(std::uint64_t t) {
    const auto ydrop = std::min<std::uint64_t>(
        t / kYmLen > ym_base_ ? t / kYmLen - ym_base_ : 0, ym_.size());
    ym_.erase(ym_.begin(), ym_.begin() + static_cast<std::ptrdiff_t>(ydrop));
    ym_raw_.erase(ym_raw_.begin(),
                  ym_raw_.begin() + static_cast<std::ptrdiff_t>(ydrop));
    ym_base_ += ydrop;
    const auto pdrop = std::min<std::uint64_t>(
        t / kPsgLen > psg_base_ ? t / kPsgLen - psg_base_ : 0, psg_.size());
    psg_.erase(psg_.begin(), psg_.begin() + static_cast<std::ptrdiff_t>(pdrop));
    psg_raw_.erase(psg_raw_.begin(),
                   psg_raw_.begin() + static_cast<std::ptrdiff_t>(pdrop));
    psg_base_ += pdrop;
  }

  std::uint64_t master_;
  std::vector<YmSample> ym_;
  std::vector<std::int16_t> psg_;
  std::vector<YmSample> ym_raw_;
  std::vector<std::int16_t> psg_raw_;
  std::uint64_t ym_base_ = 0,
                psg_base_ = 0; ///< index of the first stored sample
  std::uint64_t next_ = 0;     ///< next output sample
  std::vector<std::int16_t> out_;
};

} // namespace tessera::md
