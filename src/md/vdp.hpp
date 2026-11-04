// Minimal boot VDP of the native Mega Drive core (spec 008, TP-101.9).
// Accepts the control and data ports, keeps VRAM/CRAM/VSRAM, runs the three
// DMA modes instantly, and produces the status flags, the vertical and
// horizontal interrupts and the V/H counters. It draws nothing: rendering is
// TP-104. Counters and DMA timing are provisional until the differential
// bench compares them with gpgx (TP-105, TP-203).
#pragma once
#include "md/machine.hpp"

#include <array>
#include <cstdint>
#include <functional>

namespace tessera::md {

class Vdp {
public:
  explicit Vdp(Standard standard) noexcept : standard_(standard) {}

  // Ports (16 bit values; a byte access is the byte repeated in both halves).
  void write_control(std::uint16_t value);
  void write_data(std::uint16_t value);
  std::uint16_t read_data();
  std::uint16_t read_status();
  /// HV counter word; `line_cycle` is the master cycle inside the current line.
  std::uint16_t read_hv(std::uint32_t line_cycle) const noexcept;
  /// Freezes the HV counter at `line_cycle` of the current line. Ignored
  /// unless register 0 bit 1 (latch enable) is set; the frozen value is what
  /// read_hv returns until the bit is cleared.
  void latch_hv(std::uint32_t line_cycle) noexcept;

  /// Called at the start of every line, 0-based within the frame.
  void begin_line(std::uint32_t line);
  /// Highest pending interrupt level that is enabled (6 VINT, 4 HINT), or 0.
  std::uint8_t irq_level() const noexcept;
  void acknowledge(std::uint8_t level) noexcept;

  /// Master cycle of the access being made; the owner sets it before every
  /// port access so that DMA durations and the busy bit have a time base.
  void set_time(std::uint64_t master) noexcept { now_ = master; }
  /// Master cycles the 68000 stalled in a DMA that read its bus; returns the
  /// amount once and clears it.
  std::uint64_t take_cpu_stall() noexcept {
    const auto stall = cpu_stall_;
    cpu_stall_ = 0;
    return stall;
  }

  /// DMA source: reads a word from the 68000 bus.
  void set_dma_reader(std::function<std::uint16_t(std::uint32_t)> reader) {
    dma_reader_ = std::move(reader);
  }

  std::uint8_t reg(unsigned index) const noexcept { return regs_[index & 31U]; }
  const std::array<std::uint8_t, 0x10000> &vram() const noexcept {
    return vram_;
  }
  const std::array<std::uint16_t, 64> &cram() const noexcept { return cram_; }
  const std::array<std::uint16_t, 40> &vsram() const noexcept { return vsram_; }
  std::uint32_t line() const noexcept { return line_; }

private:
  std::uint32_t active_lines() const noexcept;
  void put(std::uint16_t value);
  void run_dma(std::uint16_t first_data);
  std::uint16_t live_hv(std::uint32_t line_cycle) const noexcept;
  bool display_blank() const noexcept;
  /// Register 1 bit 2 clear: the SMS compatible Mode 4 (Power Base Converter).
  bool mode4() const noexcept { return (regs_[1] & 0x04U) == 0; }
  /// Master cycles that `slots` external-bus slots last in the current mode.
  std::uint64_t dma_cycles(std::uint64_t slots) const noexcept;
  std::uint32_t slots_per_line() const noexcept;

  Standard standard_;
  std::array<std::uint8_t, 32> regs_{};
  std::array<std::uint8_t, 0x10000> vram_{};
  std::array<std::uint16_t, 64> cram_{};
  std::array<std::uint16_t, 40> vsram_{};
  std::uint8_t code_ = 0;
  std::uint16_t address_ = 0;
  std::uint16_t first_word_ = 0;
  bool pending_second_ = false;
  bool dma_pending_ = false;
  std::uint32_t line_ = 0;
  bool vint_flag_ = false;
  bool hint_flag_ = false;
  bool hv_latched_ = false;
  std::uint16_t hv_latch_value_ = 0;
  std::int32_t hint_counter_ = 0;
  std::uint64_t now_ = 0;
  std::uint64_t busy_until_ = 0;
  std::uint64_t cpu_stall_ = 0;
  std::function<std::uint16_t(std::uint32_t)> dma_reader_;
};

} // namespace tessera::md
