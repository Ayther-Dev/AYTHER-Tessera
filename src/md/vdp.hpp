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
  /// The VRAM as it was when the current line started (begin_line_vram):
  /// what gpgx draws the planes and the window from (measured, raster_bands
  /// 23 and 32). Copied on the first VRAM write of a line, so a line without
  /// writes costs nothing.
  const std::array<std::uint8_t, 0x10000> &vram_at_line_start() const noexcept {
    return line_vram_written_ ? line_vram_ : vram_;
  }
  void begin_line_vram() noexcept { line_vram_written_ = false; }
  /// FRAME_DELTA dirty patterns (TP-205.4): one byte per 32-byte pattern of
  /// the VRAM, bit n set when a write changed a byte of its row n (4 bytes a
  /// row). Writing the value already there marks nothing, except through a
  /// VRAM fill or copy (measured on gpgx).
  /// Gathered since the last take_dirty; observation only, not saved.
  using DirtyPatterns = std::array<std::uint8_t, 2048>;
  DirtyPatterns take_dirty() noexcept {
    const auto out = dirty_;
    dirty_ = {};
    return out;
  }
  const std::array<std::uint16_t, 64> &cram() const noexcept { return cram_; }
  const std::array<std::uint16_t, 40> &vsram() const noexcept { return vsram_; }
  /// CRAM entries 16, 32 and 48 as they were the last time the limited
  /// palette or shadow/highlight bit changed (0 until then): what gpgx shows
  /// for them in the PALETTE region; later CRAM writes do not reach it.
  const std::array<std::uint16_t, 3> &transparent_cram() const noexcept {
    return transparent_cram_;
  }
  std::uint32_t line() const noexcept { return line_; }
  /// The width (register 12) or the V30 bit changed since the frame started
  /// (its first line, in the vertical blanking): the SYSTEM GEOMETRY_PENDING
  /// flag (measured on gpgx, raster_bands 7 and 15 to 18; rewriting the same
  /// value does not count).
  bool geometry_changed() const noexcept { return geometry_changed_; }

  /// RASTER_JOURNAL and RASTER_FALLBACK_REASONS of the frame (measured on
  /// gpgx, raster_bands 0 to 33). A write is an event when it changes a value
  /// and the line it first shows in (the current line + 1) is a visible line:
  /// reason 1 a register (all but 10, 15 and the DMA ones; address = index),
  /// 2 a CRAM entry (address = entry index, data = BBBGGGRRR), 4 a VSRAM
  /// word (byte address), 8 a word of the horizontal scroll table (VRAM
  /// address). Other VRAM writes leave no event but set 0x40; a change of
  /// width sets 0x20 and a full journal 0x80. Observation only: not in the
  /// savestate.
  struct RasterEvent {
    std::uint16_t v_counter = 0;
    std::uint16_t reason = 0;
    std::uint16_t address = 0;
    std::uint16_t data = 0;
  };
  struct RasterTracking {
    std::array<RasterEvent, 256> events{};
    std::uint32_t count = 0;
    std::uint32_t dropped = 0;
    std::uint32_t fallback = 0;
    bool cram_changed = false; ///< a CRAM change on a visible line
  };
  const RasterTracking &raster() const noexcept { return raster_; }
  /// Puts back the value a journal event wrote (the replay of
  /// recompose_multilayer with RASTER_TRACKING), with no other effect.
  void replay(const RasterEvent &event) noexcept;
  /// Called at the first line of a frame, before the CPU runs in it: clears
  /// the raster tracking and fixes the frame's active height. The horizontal
  /// interrupt counter and the journal keep that height for the whole frame,
  /// so V30 set in the vertical interrupt does not turn lines 225 to 239 of
  /// the blanking into active lines (measured, raster_bands 18).
  void begin_frame_tracking() noexcept {
    raster_ = {};
    frame_active_ = active_lines();
  }
  /// Window clip of LINE_REGS (first and end 2-cell column): set when
  /// register 17 is written with a non-zero column count, whatever the line,
  /// and kept otherwise, so a write in the last line's horizontal interrupt
  /// shows in the next frame's first lines (measured, raster_bands 30).
  /// Observation only: not in the savestate.
  std::array<std::uint8_t, 2> window_clip() const noexcept {
    return window_clip_;
  }
  /// Active height fixed at the start of the frame (224 or 240).
  std::uint32_t frame_active_lines() const noexcept { return frame_active_; }
  void clear_geometry_changed() noexcept { geometry_changed_ = false; }

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
  std::uint64_t fill_end(std::uint64_t start, std::uint64_t slots,
                         bool copy) const noexcept;
  std::uint64_t slot_service(std::uint64_t start,
                             unsigned slots) const noexcept;
  std::uint16_t live_hv(std::uint32_t line_cycle) const noexcept;
  bool display_blank() const noexcept;
  bool blank_at(std::uint64_t t) const noexcept;
  /// Register 1 bit 2 clear: the SMS compatible Mode 4 (Power Base Converter).
  bool mode4() const noexcept { return (regs_[1] & 0x04U) == 0; }
  /// Moves the VRAM between the Mode 5 and the Mode 4 layouts (vdp.cpp).
  void remap_vram(bool to_mode4);
  /// Master cycles that `slots` external-bus slots last in the current mode.
  std::uint64_t dma_cycles(std::uint64_t slots) const noexcept;
  std::uint32_t slots_per_line() const noexcept;

  Standard standard_;
  // Register 10 (H interrupt counter) powers on at $FF in the reference core
  // (measured on the regions of palette_sweep: no register write touches it).
  std::array<std::uint8_t, 32> regs_ = [] {
    std::array<std::uint8_t, 32> r{};
    r[10] = 0xFF;
    return r;
  }();
  std::array<std::uint8_t, 0x10000> vram_{};
  void touch_vram() {
    if (!line_vram_written_) {
      line_vram_ = vram_;
      line_vram_written_ = true;
    }
  }
  /// Writes a VRAM byte and marks its pattern row when the value changes,
  /// or always for a DMA fill or copy (`always`; measured on gpgx: the same
  /// value through the data port or a 68000 DMA marks nothing).
  void set_vram(std::uint32_t index, std::uint8_t value,
                bool always = false) noexcept {
    index &= 0xFFFFU;
    if (vram_[index] == value && !always)
      return;
    vram_[index] = value;
    dirty_[index >> 5] |= static_cast<std::uint8_t>(1U << ((index >> 2) & 7U));
  }
  DirtyPatterns dirty_{};
  // Not in the savestate: begin_line_vram resets it before any line is drawn.
  std::array<std::uint8_t, 0x10000> line_vram_{};
  bool line_vram_written_ = false;
  RasterTracking raster_;
  /// The visible line a write made now first shows in, or -1.
  int raster_line() const noexcept;
  void journal(std::uint16_t reason, std::uint16_t address, std::uint16_t data,
               bool display_toggle = false) noexcept;
  void track_vram_write(std::uint32_t address, std::uint16_t before,
                        std::uint16_t after) noexcept;
  std::array<std::uint16_t, 64> cram_{};
  std::array<std::uint16_t, 40> vsram_{};
  std::array<std::uint16_t, 3> transparent_cram_{};
  bool geometry_changed_ = false;
  std::uint8_t code_ = 0;
  std::uint16_t address_ = 0;
  std::uint16_t first_word_ = 0;
  bool pending_second_ = false;
  bool dma_pending_ = false;
  std::uint32_t line_ = 0;
  // Not in the savestate: set again at every frame start, and from the
  // registers on load.
  std::uint32_t frame_active_ = 224;
  std::array<std::uint8_t, 2> window_clip_{};
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
  // Data of the last four words through the data port (CPU writes, 68000
  // DMA words, the fill word), and the entry the next one goes to. A CRAM,
  // VSRAM or 8-bit VRAM read takes the bits the memory does not drive from
  // that entry (measured on gpgx with VDP FIFO Testing, TP-104.3).
  std::array<std::uint16_t, 4> fifo_data_{};
  unsigned fifo_wr_ = 0;
  void fifo_record(std::uint16_t value) noexcept {
    fifo_data_[fifo_wr_] = value;
    fifo_wr_ = (fifo_wr_ + 1U) & 3U;
  }
  std::function<std::uint16_t(std::uint32_t)> dma_reader_;
};

} // namespace tessera::md
