// Native Sega Master System / Game Gear / SG-1000 machine
// (spec 008, Phase 6 / TP-601.1..TP-601.5).
#pragma once
#include "md/psg.hpp"
#include "md/state_io.hpp"
#include "md/z80.hpp"
#include "sms/vdp_m4.hpp"
#include "sms/ym2413.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace tessera::sms {

enum class SmsMapper : std::uint8_t { plain = 0, sega, codemasters, korean };

inline constexpr std::uint16_t kSmsUp = 0x01;
inline constexpr std::uint16_t kSmsDown = 0x02;
inline constexpr std::uint16_t kSmsLeft = 0x04;
inline constexpr std::uint16_t kSmsRight = 0x08;
inline constexpr std::uint16_t kSmsButton1 = 0x10;
inline constexpr std::uint16_t kSmsButton2 = 0x20;
inline constexpr std::uint16_t kSmsPauseOrStart = 0x40;

class System final : private md::Z80Bus {
public:
  System(std::vector<std::uint8_t> rom, ConsoleModel model, bool pal = false,
         bool japanese = false, SmsMapper mapper = SmsMapper::sega);

  void reset();
  void run_frame();

  void set_pad(unsigned port, std::uint16_t pressed) noexcept;

  /// PQ-15 AUDIO_MUTE for Mode 4 / TMS systems:
  /// - bits 0..8: YM2413 FM channels 0..8
  /// - bits 9..12: PSG SN76489 channels 0..3 (tones 0..2, noise)
  /// - bits 13..17: YM2413 rhythm channels (BD, SD, TOM, TCY, HH)
  static constexpr std::uint32_t kAudioMuteBits = 0x3FFFFU;
  void set_audio_mute(std::uint32_t mask) noexcept;
  std::uint32_t audio_mute() const noexcept { return audio_mute_; }

  std::uint64_t frame() const noexcept { return frame_; }
  std::uint32_t rom_crc32() const noexcept { return rom_crc32_; }
  ConsoleModel model() const noexcept { return model_; }
  SmsMapper mapper() const noexcept { return mapper_; }

  const VdpM4 &vdp() const noexcept { return vdp_; }
  const Ym2413 &ym2413() const noexcept { return ym2413_; }
  std::span<const std::int16_t> audio() const noexcept { return audio_out_; }
  std::span<const std::uint8_t> ram() const noexcept { return ram_; }
  std::span<const std::uint8_t> sram() const noexcept { return sram_; }

  bool load_srm(std::span<const std::uint8_t> file);
  std::vector<std::uint8_t> to_srm() const;
  bool save_srm_atomic(const std::string &path);

  std::vector<std::uint8_t> save_state() const;
  bool load_state(std::span<const std::uint8_t> state);

private:
  std::uint8_t read(std::uint16_t addr, unsigned &wait) override;
  void write(std::uint16_t addr, std::uint8_t value, unsigned &wait) override;
  std::uint8_t in(std::uint16_t port) override;
  void out(std::uint16_t port, std::uint8_t value) override;

  std::uint8_t read_rom_mapped(std::uint16_t addr) const noexcept;
  void write_mapper(std::uint16_t addr, std::uint8_t value) noexcept;

  std::vector<std::uint8_t> rom_;
  ConsoleModel model_;
  bool pal_;
  bool japanese_;
  SmsMapper mapper_;
  std::uint32_t rom_crc32_ = 0;

  VdpM4 vdp_;
  md::Z80 z80_;
  md::Psg psg_;
  Ym2413 ym2413_;

  std::array<std::uint8_t, 0x2000> ram_{};
  std::array<std::uint8_t, 0x8000> sram_{};
  bool sram_dirty_ = false;

  std::array<std::uint8_t, 4> sega_regs_{0, 0, 1, 2};
  std::array<std::uint8_t, 3> cm_banks_{0, 1, 2};
  std::uint8_t korean_bank_ = 2;

  std::uint8_t mem_ctrl_ = 0;
  std::uint8_t io_ctrl_ = 0xFF;
  std::uint8_t gg_stereo_ = 0xFF;
  std::array<std::uint16_t, 2> pads_{0, 0};
  bool prev_pause_ = false;

  std::uint64_t frame_ = 0;
  std::uint64_t master_ = 0;
  std::uint32_t audio_mute_ = 0;
  std::vector<std::int16_t> audio_out_;
};

} // namespace tessera::sms
