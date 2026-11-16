// Sega Master System / Game Gear (Mode 4) and SG-1000 (TMS9918A) VDP
// (spec 008, Phase 6 / TP-601.2).
#pragma once
#include "md/state_io.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace tessera::sms {

enum class ConsoleModel : std::uint8_t { sms = 0, game_gear, sg1000 };

class VdpM4 {
public:
  static constexpr std::uint32_t kSmsWidth = 256;
  static constexpr std::uint32_t kSmsHeight = 192;
  static constexpr std::uint32_t kGgWidth = 160;
  static constexpr std::uint32_t kGgHeight = 144;

  explicit VdpM4(ConsoleModel model = ConsoleModel::sms) noexcept;

  void reset() noexcept;

  std::uint8_t read_data() noexcept;
  std::uint8_t read_control() noexcept;
  void write_data(std::uint8_t value) noexcept;
  void write_control(std::uint8_t value) noexcept;

  std::uint8_t v_counter() const noexcept { return v_counter_; }
  std::uint8_t h_counter() const noexcept { return h_counter_; }
  void set_counters(std::uint8_t v, std::uint8_t h) noexcept {
    v_counter_ = v;
    h_counter_ = h;
  }

  /// Renders scanline `line` (0..191) and advances line IRQ state.
  void render_line(int line) noexcept;
  void start_frame() noexcept;
  void end_active_display() noexcept;

  bool irq_pending() const noexcept { return vint_pending_ || hint_pending_; }

  std::uint32_t width() const noexcept {
    return model_ == ConsoleModel::game_gear ? kGgWidth : kSmsWidth;
  }
  std::uint32_t height() const noexcept {
    return model_ == ConsoleModel::game_gear ? kGgHeight : kSmsHeight;
  }
  std::span<const std::uint32_t> framebuffer() const noexcept { return fb_; }
  std::span<const std::uint8_t> vram() const noexcept { return vram_; }
  std::span<const std::uint8_t> cram() const noexcept { return cram_; }

  void save(md::StateWriter &w) const;
  void load(md::StateReader &r);

private:
  void render_mode4_line(int line) noexcept;
  void render_tms_line(int line) noexcept;
  std::uint32_t sms_palette_argb(std::uint8_t index) const noexcept;
  std::uint32_t gg_palette_argb(std::uint8_t index) const noexcept;

  ConsoleModel model_ = ConsoleModel::sms;
  std::array<std::uint8_t, 0x4000> vram_{};
  std::array<std::uint8_t, 64> cram_{};
  std::array<std::uint8_t, 16> regs_{};
  std::array<std::uint32_t, kSmsWidth * kSmsHeight> fb_{};

  std::uint16_t addr_ = 0;
  std::uint8_t code_ = 0;
  std::uint8_t latch_ = 0;
  bool control_pending_ = false;
  std::uint8_t read_buffer_ = 0;
  std::uint8_t status_ = 0;
  std::uint8_t gg_cram_latch_ = 0;

  std::uint8_t line_counter_ = 0xFF;
  std::uint8_t vscroll_latch_ = 0;
  std::uint8_t v_counter_ = 0;
  std::uint8_t h_counter_ = 0;
  bool vint_pending_ = false;
  bool hint_pending_ = false;
};

} // namespace tessera::sms
