// YM2413 (OPLL) FM sound chip for Sega Master System (spec 008, Phase 6).
// Supports register writes ($F0 addr, $F1 data), $F2 detection latch,
// AUDIO_MUTE per-channel masking (PQ-15: bits 0..8 FM, bits 9..13 rhythm),
// and deterministic savestate.
#pragma once
#include "md/state_io.hpp"

#include <array>
#include <cstdint>

namespace tessera::sms {

class Ym2413 {
public:
  void reset() noexcept;

  void write_addr(std::uint8_t addr) noexcept { addr_ = addr; }
  void write_data(std::uint8_t value) noexcept;

  /// $F2 detection port on Master System (tracks bits 0..2 written by
  /// software).
  void write_detect(std::uint8_t value) noexcept { detect_ = value & 0x07U; }
  std::uint8_t read_detect() const noexcept { return detect_; }

  void set_mute(std::uint16_t mask14) noexcept { mute_ = mask14 & 0x3FFFU; }
  std::uint16_t mute() const noexcept { return mute_; }

  /// Generates one stereo sample pair at 48 kHz (muted according to `mute_`).
  void step_sample(std::int32_t &left, std::int32_t &right) noexcept;

  std::uint8_t reg(std::uint8_t idx) const noexcept {
    return idx < regs_.size() ? regs_[idx] : 0;
  }

  void save(md::StateWriter &w) const;
  void load(md::StateReader &r);

private:
  std::array<std::uint8_t, 0x40> regs_{};
  std::array<std::uint64_t, 9> phase_{};
  std::uint8_t addr_ = 0;
  std::uint8_t detect_ = 0;
  std::uint16_t mute_ = 0;
};

} // namespace tessera::sms
