// Minimal boot VDP of the native Mega Drive core (spec 008, TP-101.9).
// Accepts the control and data ports, keeps VRAM/CRAM/VSRAM, runs the three
// DMA modes instantly, and produces the status flags, the vertical and
// horizontal interrupts and the V/H counters. It draws nothing: rendering is
// TP-104. Counters and DMA timing are provisional until the differential
// bench compares them with gpgx (TP-105, TP-203).
#pragma once
#include "md/machine.hpp"
#include "md/state_io.hpp"

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
  // `raise_vint` false leaves the vertical interrupt to the caller, which
  // raises it `vint_cycle()` master cycles into the line (gpgx).
  void begin_line(std::uint32_t line, bool raise_vint = true);
  std::uint32_t vint_cycle() const noexcept {
    return (regs_[12] & 0x81U) != 0 ? 788U : 770U;
  }
  void raise_vint() noexcept { vint_flag_ = true; }
  /// Status bit 4: odd field of an interlaced frame.
  void set_odd_field(bool odd) noexcept { odd_field_ = odd; }
  /// Sprite overflow (status bit 6) and collision (bit 5), set by the
  /// renderer and cleared by a status read.
  void raise_sprite_flags(bool overflow, bool collision) noexcept {
    sprite_overflow_ |= overflow;
    sprite_collision_ |= collision;
  }
  /// Highest pending interrupt level that is enabled (6 VINT, 4 HINT), or 0.
  std::uint8_t irq_level() const noexcept;
  void acknowledge(std::uint8_t level) noexcept;

  /// Master cycle of the access being made; the owner sets it before every
  /// port access so that DMA durations and the busy bit have a time base.
  void set_time(std::uint64_t master) noexcept { now_ = master; }
  /// Master cycle at which the current line starts (the HV counter origin).
  void set_line_start(std::uint64_t master) noexcept { line_start_ = master; }
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

  /// Register 1 bit 2 clear: the SMS compatible Mode 4 (Power Base Converter).
  bool mode4_active() const noexcept { return mode4(); }
  std::uint8_t reg(unsigned index) const noexcept { return regs_[index & 31U]; }
  const std::array<std::uint8_t, 0x10000> &vram() const noexcept {
    return vram_;
  }
  const std::array<std::uint16_t, 64> &cram() const noexcept { return cram_; }
  const std::array<std::uint16_t, 40> &vsram() const noexcept { return vsram_; }
  std::uint32_t line() const noexcept { return line_; }

  // Line a frame starts at in gpgx: the first line of the vertical blanking,
  // i.e. the active height (192 in Mode 4, 224 or 240 in Mode 5).
  std::uint32_t frame_start_line() const noexcept {
    return mode4() ? 192U : active_lines();
  }
  // gpgx has no vertical interrupt on the first frame after a VDP reset.
  // Measured on gpgx: no horizontal interrupt reaches the CPU on that frame.
  void cancel_hint() noexcept { hint_flag_ = false; }
  // gpgx wakes up with its line counter at $FF and only reloads it from
  // register 10 once the first frame is over.
  void power_on_hint_counter() noexcept { hint_counter_ = 0xFFU; }

  /// Savestate: registers, memories, ports, flags, counters and the FIFO (the
  /// video standard and the DMA reader belong to the machine).
  void save(StateWriter &w) const;
  void load(StateReader &r);

private:
  std::uint32_t active_lines() const noexcept;
  void put(std::uint16_t value);
  void run_dma(std::uint16_t first_data);
  void finish_dma(std::uint32_t length, bool advance_source) noexcept;
  /// CPU data port write through the 4 entry FIFO (TP-104.3).
  void fifo_write(std::uint16_t value);
  void fifo_retire() noexcept;
  /// Master cycle of the first external slot after `after` (display drawn).
  std::uint64_t next_slot(std::uint64_t after, bool vram) const noexcept;
  std::uint64_t slots_end(std::uint64_t start, std::uint64_t slots,
                          bool colour_ram = false) const noexcept;
  std::uint64_t slot_service(std::uint64_t start,
                             unsigned slots) const noexcept;
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
  bool odd_field_ = false;
  bool sprite_overflow_ = false;
  bool sprite_collision_ = false;
  bool hint_flag_ = false;
  bool hv_latched_ = false;
  std::uint16_t hv_latch_value_ = 0;
  std::int32_t hint_counter_ = 0;
  std::uint64_t now_ = 0;
  std::uint64_t line_start_ = 0;
  std::uint64_t busy_until_ = 0;
  std::uint64_t cpu_stall_ = 0;
  std::array<std::uint64_t, 4> fifo_done_{};
  unsigned fifo_head_ = 0;
  unsigned fifo_count_ = 0;
  std::uint64_t fifo_last_done_ = 0;
  std::function<std::uint16_t(std::uint32_t)> dma_reader_;
};

} // namespace tessera::md
