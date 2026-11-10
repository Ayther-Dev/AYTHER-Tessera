#include "md/vdp.hpp"

#include <algorithm>

namespace tessera::md {

std::uint32_t Vdp::active_lines() const noexcept {
  return (regs_[1] & 0x08U) != 0 ? 240U : 224U;
}

void Vdp::write_control(std::uint16_t value) {
  if (mode4() && !pending_second_ && (value & 0xC000U) != 0x8000U) {
    // gpgx (vdp_68k_ctrl_w): in mode 4 there is no second command word, every
    // control word is a complete command, so a trailing 0 word of a long write
    // re-points the address to 0 with code 0.
    address_ = static_cast<std::uint16_t>(value & 0x3FFFU);
    code_ = static_cast<std::uint8_t>((code_ & 0x3CU) | ((value >> 14) & 3U));
    return;
  }
  if (pending_second_) {
    pending_second_ = false;
    code_ = static_cast<std::uint8_t>(((first_word_ >> 14) & 3U) |
                                      ((value >> 2) & 0x3CU));
    address_ = static_cast<std::uint16_t>((first_word_ & 0x3FFFU) |
                                          ((value & 3U) << 14));
    // A DMA command only starts when DMA is enabled (register 1, bit 4).
    if ((code_ & 0x20U) != 0 && (regs_[1] & 0x10U) != 0) {
      if ((regs_[23] >> 6) == 2)
        dma_pending_ = true; // VRAM fill waits for the data word
      else
        run_dma(0);
    }
    return;
  }
  if ((value & 0xC000U) == 0x8000U) {
    const unsigned index = (value >> 8) & 0x1FU;
    // In mode 4 (register 1 bit 2 clear) registers above 10 do not exist.
    const bool was_mode4 = mode4();
    const std::uint8_t palette_bits_before =
        static_cast<std::uint8_t>((regs_[0] & 0x04U) | (regs_[12] & 0x08U));
    // Geometry: H40 (12), V30 (1 bit 3) and the mode (1 bit 2): leaving
    // Mode 4 takes the frame from 192 lines to 224 (measured, mode4_pbc).
    const std::uint8_t geometry_before =
        static_cast<std::uint8_t>((regs_[12] & 0x81U) | (regs_[1] & 0x0CU));
    const std::uint8_t old_value = regs_[index];
    if (index <= 10 || (regs_[1] & 0x04U) != 0) {
      regs_[index] = static_cast<std::uint8_t>(value);
      // Not events: the line counter (10), the auto-increment (15) and the
      // DMA registers (19 and up) (measured, raster_bands 11, 19, 20).
      if (regs_[index] != old_value && index != 10 && index != 15 &&
          index < 19) {
        journal(1, static_cast<std::uint16_t>(index), regs_[index],
                index == 1 && ((old_value ^ regs_[index]) & 0x40U) != 0);
        if (index == 12 && ((old_value ^ regs_[index]) & 0x81U) != 0 &&
            raster_line() >= 0 && (regs_[1] & 0x40U) != 0)
          raster_.fallback |= 0x20U; // width change
      }
      if (index == 17 && (regs_[17] & 0x1FU) != 0) {
        const std::uint8_t width = (regs_[12] & 0x81U) != 0 ? 20U : 16U;
        const auto split = static_cast<std::uint8_t>(
            std::min<unsigned>(regs_[17] & 0x1FU, width));
        window_clip_ = (regs_[17] & 0x80U) != 0
                           ? std::array<std::uint8_t, 2>{split, width}
                           : std::array<std::uint8_t, 2>{0, split};
      }
    }
    if (((regs_[12] & 0x81U) | (regs_[1] & 0x0CU)) != geometry_before)
      geometry_changed_ = true;
    if (((regs_[0] & 0x04U) | (regs_[12] & 0x08U)) != palette_bits_before) {
      // gpgx refreshes its whole palette when the limited palette or the
      // shadow/highlight bit changes; only then do the transparent entries
      // of palettes 1 to 3 take the CRAM value (measured, raster_bands 12-14).
      transparent_cram_ = {cram_[16], cram_[32], cram_[48]};
    }
    if (was_mode4 != mode4())
      remap_vram(mode4());
    if ((regs_[0] & 0x02U) == 0)
      hv_latched_ = false; // clearing the latch enable releases the counter
    code_ = 0;
    return;
  }
  first_word_ = value;
  pending_second_ = true;
  // The low bits of the first word are already visible to a data access.
  address_ =
      static_cast<std::uint16_t>((address_ & 0xC000U) | (value & 0x3FFFU));
  code_ = static_cast<std::uint8_t>((code_ & 0x3CU) | ((value >> 14) & 3U));
}

void Vdp::put(std::uint16_t value) {
  if (mode4() && (code_ & 0x02U) != 0) {
    // Mode 4 CRAM write (gpgx): the word index is the address & $1F, the data
    // is packed to 9 bits (xxxBBGGRR in the low 6 bits plus the top blue bits)
    // and a Mode 5 read gives it back with the zero bits in between.
    const unsigned packed = ((value & 0xE00U) >> 3) | (value & 0x3FU);
    cram_[address_ & 0x1FU] = static_cast<std::uint16_t>(
        ((packed & 0x1C0U) << 3) | ((packed & 0x38U) << 2) |
        ((packed & 7U) << 1));
    address_ =
        static_cast<std::uint16_t>((address_ + regs_[15] + 1U) & 0x3FFFU);
    return;
  }
  if (mode4() && (code_ & 0x02U) == 0) {
    // Mode 4 VRAM write (gpgx vdp_68k_data_w_m4): the word goes to the even
    // address, byte swapped when the address is odd, and the address steps by
    // register 15 + 1.
    touch_vram();
    const std::uint32_t a = address_ & 0x3FFFU;
    const std::uint32_t index = a & 0x3FFEU;
    const auto word = static_cast<std::uint16_t>(
        (a & 1U) != 0 ? ((value >> 8) | (value << 8)) : value);
    // gpgx keeps the low byte at the even address (SMS layout; measured on
    // its VRAM region with mode4_scene).
    set_vram(index, static_cast<std::uint8_t>(word));
    set_vram(index + 1U, static_cast<std::uint8_t>(word >> 8));
    address_ = static_cast<std::uint16_t>((a + regs_[15] + 1U) & 0x3FFFU);
    return;
  }
  switch (code_ & 0x0FU) {
  case 1: { // VRAM write
    touch_vram();
    const std::uint32_t word_at = address_ & 0xFFFEU;
    const auto before = static_cast<std::uint16_t>(
        (std::uint32_t{vram_[word_at]} << 8) | vram_[word_at + 1U]);
    // odd address: bytes swapped (address ^ 1 is then address - 1)
    set_vram(address_, static_cast<std::uint8_t>(value >> 8));
    set_vram(address_ ^ 1U, static_cast<std::uint8_t>(value));
    track_vram_write(
        word_at, before,
        static_cast<std::uint16_t>((std::uint32_t{vram_[word_at]} << 8) |
                                   vram_[word_at + 1U]));
    break;
  }
  case 3: {
    auto &entry = cram_[(address_ >> 1) & 0x3FU];
    const auto colour = static_cast<std::uint16_t>(value & 0x0EEEU);
    if (entry != colour) {
      entry = colour;
      const auto packed = static_cast<std::uint16_t>(((colour >> 9 & 7U) << 6) |
                                                     ((colour >> 5 & 7U) << 3) |
                                                     (colour >> 1 & 7U));
      if (raster_line() >= 0)
        raster_.cram_changed = true;
      // The address of a CRAM event is the entry index (raster_bands 33).
      journal(2, static_cast<std::uint16_t>((address_ >> 1) & 0x3FU), packed);
    }
    break;
  }
  case 5:
    if (((address_ >> 1) & 0x7FU) < vsram_.size()) {
      auto &entry = vsram_[(address_ >> 1) & 0x7FU];
      const auto word = static_cast<std::uint16_t>(value & 0x07FFU);
      if (entry != word) {
        entry = word;
        journal(4, static_cast<std::uint16_t>(address_ & 0x7EU), word);
      }
    }
    break;
  default:
    break;
  }
  address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
}

void Vdp::replay(const RasterEvent &event) noexcept {
  switch (event.reason) {
  case 1:
    regs_[event.address & 31U] = static_cast<std::uint8_t>(event.data);
    break;
  case 2: // BBBGGGRRR back to the CRAM word
    cram_[event.address & 0x3FU] = static_cast<std::uint16_t>(
        ((event.data >> 6 & 7U) << 9) | ((event.data >> 3 & 7U) << 5) |
        ((event.data & 7U) << 1));
    break;
  case 4:
    if ((event.address >> 1) < vsram_.size())
      vsram_[event.address >> 1] = event.data;
    break;
  case 8:
    vram_[event.address & 0xFFFEU] = static_cast<std::uint8_t>(event.data >> 8);
    vram_[(event.address & 0xFFFEU) + 1U] =
        static_cast<std::uint8_t>(event.data);
    break;
  default:
    break;
  }
}

// gpgx keeps a single VRAM and Mode 4 sees it through a permutation: Mode 4
// word k (bytes 2k and 2k + 1 of its region) is Mode 5 word mode4_word(k)
// with its two bytes swapped. Below $2000 the low bit of the word moves to
// bit 8 within each block of 512 words; above, only the bytes swap
// (measured on gpgx over the 32768 words, TP-209.1 (d): a Mode 5 -> 4 -> 5
// round trip leaves the VRAM as it was; mode4_pbc's writes land in Mode 5
// word 2k because they stay below word $100). Tessera stores the VRAM in the
// layout of the current mode, so a mode change applies the permutation.
namespace {
constexpr std::uint32_t mode4_word(std::uint32_t k) noexcept {
  return k < 0x2000U ? (k & ~0x1FFU) | ((k & 0xFFU) << 1) | ((k >> 8) & 1U) : k;
}
} // namespace

void Vdp::remap_vram(bool to_mode4) {
  touch_vram();
  std::array<std::uint8_t, 0x10000> out{};
  for (std::uint32_t k = 0; k < 0x8000U; ++k) {
    const std::uint32_t w = mode4_word(k);
    if (to_mode4) {
      out[2U * k] = vram_[2U * w + 1U];
      out[2U * k + 1U] = vram_[2U * w];
    } else {
      out[2U * w] = vram_[2U * k + 1U];
      out[2U * w + 1U] = vram_[2U * k];
    }
  }
  for (std::uint32_t i = 0; i < out.size(); ++i)
    set_vram(i, out[i]);
}

int Vdp::raster_line() const noexcept {
  const std::uint32_t total = lines_per_frame(standard_);
  const std::uint32_t v = (line_ + 1U) % total;
  return v < frame_active_ ? static_cast<int>(v) : -1;
}

void Vdp::journal(std::uint16_t reason, std::uint16_t address,
                  std::uint16_t data, bool display_toggle) noexcept {
  const int v = raster_line();
  if (v < 0 || mode4())
    return;
  // With the display off a line is the backdrop: only what changes it is an
  // event, register 7, its CRAM entry and the display bit itself (measured on
  // gpgx while the test ROMs load their scene, TP-205.3).
  if ((regs_[1] & 0x40U) == 0 && !display_toggle &&
      !(reason == 1 && address == 7) &&
      !(reason == 2 && address == (regs_[7] & 0x3FU)))
    return;
  raster_.fallback |= reason;
  if (raster_.count < raster_.events.size()) {
    raster_.events[raster_.count++] = {static_cast<std::uint16_t>(v), reason,
                                       address, data};
  } else {
    ++raster_.dropped;
    raster_.fallback |= 0x80U;
  }
}

void Vdp::track_vram_write(std::uint32_t address, std::uint16_t before,
                           std::uint16_t after) noexcept {
  // With the display off no VRAM write is an event nor a fallback reason
  // (measured on gpgx, see journal()).
  if (raster_line() < 0 || mode4() || (regs_[1] & 0x40U) == 0)
    return;
  // The horizontal scroll table: up to 240 lines of two words (assumed
  // extent; raster_bands 3 and 10 write its first entry).
  const std::uint32_t table = std::uint32_t{regs_[13] & 0x3FU} << 10;
  if (address >= table && address < table + 240U * 4U) {
    if (before != after)
      journal(8, static_cast<std::uint16_t>(address), after);
  } else {
    raster_.fallback |= 0x40U; // raster_bands 22, 23, 32
  }
}

void Vdp::write_data(std::uint16_t value) {
  pending_second_ = false;
  if (dma_pending_) {
    dma_pending_ = false;
    run_dma(value);
    return;
  }
  fifo_write(value);
}

void Vdp::fifo_retire() noexcept {
  while (fifo_count_ != 0 && fifo_done_[fifo_head_] <= now_) {
    fifo_head_ = (fifo_head_ + 1U) & 3U;
    --fifo_count_;
  }
}

namespace {
// H counter values at which a stalled 68000 resumes while the display is
// drawn, one per FIFO entry retired (measured on gpgx with fifo_timing). A
// VRAM word takes two external slots, so it retires on about every other slot
// of the CRAM and VSRAM list, but not exactly: it has its own list.
constexpr std::uint8_t kCramH40[18] = {0x06, 0x0F, 0x17, 0x27, 0x2F, 0x38,
                                       0x46, 0x4E, 0x57, 0x66, 0x6F, 0x78,
                                       0x86, 0x8E, 0x97, 0xA5, 0xAE, 0xEB};
constexpr std::uint8_t kCramH32[16] = {0x05, 0x0D, 0x15, 0x25, 0x2D, 0x35,
                                       0x45, 0x4D, 0x55, 0x65, 0x6D, 0x75,
                                       0x85, 0x8C, 0x93, 0xF6};
constexpr std::uint8_t kVramH40[9] = {0x0E, 0x26, 0x36, 0x4E, 0x66,
                                      0x76, 0x8E, 0xA5, 0xEB};
constexpr std::uint8_t kVramH32[8] = {0x0D, 0x26, 0x35, 0x4D,
                                      0x66, 0x75, 0x85, 0xF7};
} // namespace

std::uint64_t Vdp::next_slot(std::uint64_t after, bool vram) const noexcept {
  // The slot is where the 68000 resumes; the HV read of the probe comes 16
  // CPU cycles later, which the table values already contain.
  constexpr std::int64_t delta = 112;
  const bool h40 = (regs_[12] & 0x81U) != 0;
  const std::uint32_t steps = h40 ? 211U : 171U;
  const std::uint32_t last_low = h40 ? 0xB6U : 0x93U;
  const std::uint32_t first_high = h40 ? 0xE4U : 0xE9U;
  const std::uint32_t zero = h40 ? 780U : 760U;
  const std::uint8_t *table =
      vram ? (h40 ? kVramH40 : kVramH32) : (h40 ? kCramH40 : kCramH32);
  const unsigned count = vram ? (h40 ? 9U : 8U) : (h40 ? 18U : 16U);
  // Slots repeat every line from the line the HV counter starts at.
  const std::int64_t rel =
      static_cast<std::int64_t>(after) - static_cast<std::int64_t>(line_start_);
  const std::int64_t k0 =
      (rel >= 0 ? rel / kCyclesPerLine
                : -((-rel + kCyclesPerLine - 1) / kCyclesPerLine));
  std::uint64_t best = ~std::uint64_t{0};
  for (std::int64_t lines = k0 - 1; lines <= k0 + 2; ++lines) {
    const std::int64_t base =
        static_cast<std::int64_t>(line_start_) + lines * kCyclesPerLine;
    for (unsigned i = 0; i < count; ++i) {
      const std::uint32_t h = table[i];
      const std::uint32_t n = h <= last_low ? h : h - first_high + last_low + 1;
      const std::int64_t at =
          base + zero + (std::int64_t{n} * kCyclesPerLine + steps - 1) / steps -
          delta;
      if (at > static_cast<std::int64_t>(after) &&
          static_cast<std::uint64_t>(at) < best)
        best = static_cast<std::uint64_t>(at);
    }
  }
  return best;
}

// Master cycle at which `slots` external slots, taken from `start` on, are
// used up. While the display is drawn the slots are the discrete ones of the
// CRAM list; in the blanking they are evenly spread.
std::uint64_t Vdp::slots_end(std::uint64_t start, std::uint64_t slots,
                             bool colour_ram) const noexcept {
  const bool h40 = (regs_[12] & 0x81U) != 0;
  // In the blanking a CRAM or VSRAM word takes 3 % more than one slot
  // (measured on gpgx with dma_timing: 2175 against 2100 units for 2048).
  const std::uint32_t per_line =
      (h40 ? 205U : 167U) * (colour_ram ? 1000U : 1031U) / 1031U;
  const std::uint32_t active_per_line = h40 ? 18U : 16U;
  const std::uint32_t total_lines = lines_per_frame(standard_);
  const std::uint32_t active = active_lines();
  const bool enabled = (regs_[1] & 0x40U) != 0;
  std::uint64_t t = start;
  while (slots != 0) {
    const std::int64_t rel =
        static_cast<std::int64_t>(t) - static_cast<std::int64_t>(line_start_);
    const std::int64_t k =
        rel >= 0 ? rel / kCyclesPerLine
                 : -((-rel + kCyclesPerLine - 1) / kCyclesPerLine);
    const std::int64_t wrapped =
        ((static_cast<std::int64_t>(line_) + k) % total_lines + total_lines) %
        total_lines;
    const std::uint64_t line_end = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(line_start_) + (k + 1) * kCyclesPerLine);
    if (!enabled || (wrapped >= active && wrapped + 1 != total_lines)) {
      // blanking: evenly spread slots
      const std::uint64_t avail = (line_end - t) * per_line / kCyclesPerLine;
      if (slots <= avail || avail == 0 || true) {
        const std::uint64_t need =
            (slots * kCyclesPerLine + per_line - 1) / per_line;
        if (t + need <= line_end)
          return t + need;
      }
      slots -= avail;
      t = line_end;
      continue;
    }
    const std::uint64_t lines_left =
        active - 1U - static_cast<std::uint64_t>(wrapped);
    if (slots > 2ULL * active_per_line && lines_left > 1) {
      std::uint64_t m = (slots - 1U) / active_per_line;
      if (m > lines_left - 1)
        m = lines_left - 1;
      if (m != 0) {
        t += m * kCyclesPerLine;
        slots -= m * active_per_line;
        continue;
      }
    }
    t = next_slot(t, false);
    --slots;
  }
  return t;
}

// Completion time of an entry of `slots` external slots started at `start`.
std::uint64_t Vdp::slot_service(std::uint64_t start,
                                unsigned slots) const noexcept {
  if (display_blank())
    return start + dma_cycles(slots);
  return next_slot(start, slots == 2U);
}

void Vdp::fifo_write(std::uint16_t value) {
  fifo_retire();
  if (fifo_count_ == 4) { // full: the 68000 waits for the oldest entry
    const std::uint64_t wait = fifo_done_[fifo_head_] - now_;
    cpu_stall_ += wait;
    now_ += wait;
    fifo_retire();
  }
  const unsigned slots = (code_ & 0x0FU) == 1 ? 2U : 1U; // VRAM word: 2 bytes
  const std::uint64_t start = fifo_last_done_ > now_ ? fifo_last_done_ : now_;
  fifo_last_done_ = slot_service(start, slots);
  fifo_done_[(fifo_head_ + fifo_count_) & 3U] = fifo_last_done_;
  ++fifo_count_;
  if (!mode4()) // Mode 4 writes leave the data bits alone (mode4_pbc)
    fifo_record(value);
  put(value);
}

std::uint16_t Vdp::read_data() {
  pending_second_ = false;
  if (mode4()) {
    // Mode 4 VRAM read (gpgx vdp_68k_data_r_m4): the word of the even address,
    // then the address steps by register 15 + 1.
    const std::uint32_t a = address_ & 0x3FFFU;
    const auto value =
        static_cast<std::uint16_t>((vram_[a | 1U] << 8) | vram_[a & 0x3FFEU]);
    address_ = static_cast<std::uint16_t>((a + regs_[15] + 1U) & 0x3FFFU);
    return value;
  }
  std::uint16_t value = 0;
  switch (code_ & 0x0FU) {
  case 0:
    value = static_cast<std::uint16_t>((vram_[address_ & 0xFFFEU] << 8) |
                                       vram_[(address_ & 0xFFFEU) + 1U]);
    break;
  case 8: // 9 bits of colour, the rest from the FIFO
    value = static_cast<std::uint16_t>((fifo_data_[fifo_wr_] & 0xF111U) |
                                       cram_[(address_ >> 1) & 0x3FU]);
    break;
  case 4: // 11 bits of scroll, the rest from the FIFO
    value = static_cast<std::uint16_t>(fifo_data_[fifo_wr_] & 0xF800U);
    if (((address_ >> 1) & 0x7FU) < vsram_.size())
      value |= vsram_[(address_ >> 1) & 0x7FU];
    break;
  case 12: // 8-bit VRAM read: the byte of address ^ 1, high byte from the FIFO
    value = static_cast<std::uint16_t>((fifo_data_[fifo_wr_] & 0xFF00U) |
                                       vram_[address_ ^ 1U]);
    break;
  default:
    break;
  }
  address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
  return value;
}

void Vdp::run_dma(std::uint16_t first_data) {
  std::uint32_t length = regs_[19] | (std::uint32_t{regs_[20]} << 8);
  if (length == 0)
    length = 0x10000;
  const unsigned mode = regs_[23] >> 6;
  const std::uint32_t source = ((std::uint32_t{regs_[23]} & 0x7FU) << 17) |
                               (std::uint32_t{regs_[22]} << 9) |
                               (std::uint32_t{regs_[21]} << 1);
  if (mode < 2) { // 68000 bus to VDP
    std::uint32_t at = source;
    for (std::uint32_t i = 0; i < length; ++i, at += 2) {
      const std::uint16_t word = dma_reader_ ? dma_reader_(at) : 0xFFFFU;
      fifo_record(word);
      put(word);
    }
    // A VRAM word takes two external slots, a CRAM or VSRAM word one.
    cpu_stall_ += slots_end(now_, (code_ & 0x0FU) == 1 ? 2ULL * length : length,
                            (code_ & 0x0FU) != 1) -
                  now_;
    finish_dma(length, true);
    return;
  }
  // Fill: one slot per byte; copy: two slots per byte (read and write).
  busy_until_ = now_ + dma_cycles(mode == 2 ? length : 2ULL * length);
  if (mode == 2) { // VRAM fill: the data word, then its high byte
    fifo_record(first_data);
    put(first_data);
    const auto byte = static_cast<std::uint8_t>(first_data >> 8);
    touch_vram();
    if (length != 0 && raster_line() >= 0)
      raster_.fallback |= 0x40U;
    for (std::uint32_t i = 0; i < length; ++i) {
      set_vram(address_ ^ 1U, byte, true);
      address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
    }
  } else { // VRAM copy
    auto from = static_cast<std::uint16_t>(source >> 1);
    touch_vram();
    if (length != 0 && raster_line() >= 0)
      raster_.fallback |= 0x40U;
    for (std::uint32_t i = 0; i < length; ++i) {
      set_vram(address_ ^ 1U, vram_[from ^ 1U], true);
      ++from;
      address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
    }
    finish_dma(length, true);
    return;
  }
  finish_dma(length, false);
}

// A finished DMA leaves the length registers at zero and, for the copies
// from the bus and the VRAM, the source registers (21, 22) advanced by the
// number of words moved (measured on gpgx: Sonic 1 relies on it).
void Vdp::finish_dma(std::uint32_t length, bool advance_source) noexcept {
  regs_[19] = regs_[20] = 0;
  if (advance_source) {
    const std::uint32_t next =
        ((std::uint32_t{regs_[22]} << 8 | regs_[21]) + length) & 0xFFFFU;
    regs_[21] = static_cast<std::uint8_t>(next);
    regs_[22] = static_cast<std::uint8_t>(next >> 8);
  }
}

std::uint16_t Vdp::read_status() {
  pending_second_ = false;
  std::uint16_t status = 0x3400U; // fixed bits
  fifo_retire();
  if (fifo_count_ == 0)
    status |= 0x0200U; // FIFO empty
  else if (fifo_count_ == 4)
    status |= 0x0100U; // FIFO full
  if (standard_ == Standard::pal)
    status |= 0x0001U;
  if (display_blank())
    status |= 0x0008U; // blanking: vertical blank or display disabled
  if (now_ < busy_until_)
    status |= 0x0002U; // DMA fill or copy in progress
  if (vint_flag_)
    status |= 0x0080U;
  if (odd_field_ && (regs_[12] & 0x02U) != 0)
    status |= 0x0010U;
  if (sprite_overflow_)
    status |= 0x0040U;
  if (sprite_collision_)
    status |= 0x0020U;
  sprite_overflow_ = sprite_collision_ = false;
  return status;
}

// The last line of the frame (V = $FF) already belongs to the display: the
// status flag drops and the 68000 gets the active-display slots there
// (measured on gpgx with fifo_timing, TP-104.3).
bool Vdp::display_blank() const noexcept {
  return (line_ >= active_lines() &&
          line_ + 1U != lines_per_frame(standard_)) ||
         (regs_[1] & 0x40U) == 0;
}

// External-bus slots per line: 16/18 (H32/H40) while the display is drawn,
// 167/205 while it is blanked. Provisional until compared with gpgx.
std::uint32_t Vdp::slots_per_line() const noexcept {
  const bool h40 = (regs_[12] & 0x81U) != 0;
  if (display_blank())
    return h40 ? 205U : 167U;
  return h40 ? 18U : 16U;
}

std::uint64_t Vdp::dma_cycles(std::uint64_t slots) const noexcept {
  const std::uint32_t per_line = slots_per_line();
  return slots * kCyclesPerLine / per_line;
}

std::uint16_t Vdp::read_hv(std::uint32_t line_cycle) const noexcept {
  if (hv_latched_ && (regs_[0] & 0x02U) != 0)
    return hv_latch_value_;
  return live_hv(line_cycle);
}

void Vdp::latch_hv(std::uint32_t line_cycle) noexcept {
  if ((regs_[0] & 0x02U) == 0)
    return;
  hv_latch_value_ = live_hv(line_cycle);
  hv_latched_ = true;
}

std::uint16_t Vdp::live_hv(std::uint32_t line_cycle) const noexcept {
  // The V counter jumps back once in the vertical blank: NTSC V28 from $EA to
  // $E5, PAL V28 from $102 to $CA, PAL V30 from $10A to $D2; NTSC V30 does not
  // jump. Verified against gpgx for NTSC and PAL (TP-106.1): the counter reads
  // $CA (V28) or $D2 (V30) right after $102 / $10A.
  std::uint32_t v = line_;
  const bool v30 = (regs_[1] & 0x08U) != 0;
  if (standard_ == Standard::ntsc) {
    if (!v30 && v > 0xEA)
      v = v - 0xEA + 0xE4;
  } else if (v30) {
    if (v > 0x10A)
      v = v - 0x10B + 0xD2;
  } else if (v > 0x102) {
    v = v - 0x103 + 0xCA;
  }
  // The H counter runs through 171 (H32: $00-$93, $E9-$FF) or 211 (H40:
  // $00-$B6, $E4-$FF) values per line (verified against gpgx).
  const bool h40 = (regs_[12] & 0x81U) != 0;
  const std::uint32_t steps = h40 ? 211U : 171U;
  const std::uint32_t last_low = h40 ? 0xB6U : 0x93U;
  const std::uint32_t first_high = h40 ? 0xE4U : 0xE9U;
  // The line starts at the end of the active display, not at H = 0: gpgx
  // (hvc.h) has H $85 (H32) or $A5 (H40) on the first master cycle of the line
  // and H = 0 on cycle 760 (H32) or 780 (H40).
  const std::uint32_t zero = h40 ? 780U : 760U;
  const std::uint32_t n =
      ((line_cycle + kCyclesPerLine - zero) % kCyclesPerLine) * steps /
      kCyclesPerLine;
  const std::uint32_t h =
      (n <= last_low ? n : n - last_low - 1 + first_high) & 0xFFU;
  return static_cast<std::uint16_t>(((v & 0xFFU) << 8) | h);
}

void Vdp::begin_line(std::uint32_t line, bool raise_vint) {
  line_ = line;
  if (line == 0)
    hint_counter_ = regs_[10];
  // The counter runs on the lines 0..active inclusive (gpgx); in the rest of
  // the blanking it is reloaded on every line.
  if (line <= frame_active_) {
    if (hint_counter_ == 0) {
      hint_flag_ = true;
      hint_counter_ = regs_[10];
    } else {
      --hint_counter_;
    }
  } else {
    hint_counter_ = regs_[10];
  }
  if (raise_vint && line == active_lines())
    vint_flag_ = true;
}

std::uint8_t Vdp::irq_level() const noexcept {
  if (vint_flag_ && (regs_[1] & 0x20U) != 0)
    return 6;
  if (hint_flag_ && (regs_[0] & 0x10U) != 0)
    return 4;
  return 0;
}

void Vdp::acknowledge(std::uint8_t level) noexcept {
  if (level == 6)
    vint_flag_ = false;
  else if (level == 4)
    hint_flag_ = false;
}

void Vdp::save(StateWriter &w) const {
  w.array(regs_);
  w.array(vram_);
  w.array(cram_);
  w.array(vsram_);
  w.array(transparent_cram_);
  w.boolean(geometry_changed_);
  w.u8(code_);
  w.u16(address_);
  w.u16(first_word_);
  w.boolean(pending_second_);
  w.boolean(dma_pending_);
  w.u32(line_);
  w.boolean(vint_flag_);
  w.boolean(odd_field_);
  w.boolean(sprite_overflow_);
  w.boolean(sprite_collision_);
  w.boolean(hint_flag_);
  w.boolean(hv_latched_);
  w.u16(hv_latch_value_);
  w.i32(hint_counter_);
  w.u64(now_);
  w.u64(line_start_);
  w.u64(busy_until_);
  w.u64(cpu_stall_);
  w.array(fifo_done_);
  w.u32(fifo_head_);
  w.u32(fifo_count_);
  w.u64(fifo_last_done_);
  w.array(fifo_data_);
  w.u32(fifo_wr_);
}

void Vdp::load(StateReader &r) {
  r.array(regs_);
  r.array(vram_);
  line_vram_written_ = false;
  raster_ = {};
  r.array(cram_);
  r.array(vsram_);
  r.array(transparent_cram_);
  geometry_changed_ = r.boolean();
  code_ = r.u8();
  address_ = r.u16();
  first_word_ = r.u16();
  pending_second_ = r.boolean();
  dma_pending_ = r.boolean();
  line_ = r.u32();
  frame_active_ = active_lines();
  vint_flag_ = r.boolean();
  odd_field_ = r.boolean();
  sprite_overflow_ = r.boolean();
  sprite_collision_ = r.boolean();
  hint_flag_ = r.boolean();
  hv_latched_ = r.boolean();
  hv_latch_value_ = r.u16();
  hint_counter_ = r.i32();
  now_ = r.u64();
  line_start_ = r.u64();
  busy_until_ = r.u64();
  cpu_stall_ = r.u64();
  r.array(fifo_done_);
  fifo_head_ = r.u32();
  fifo_count_ = r.u32();
  fifo_last_done_ = r.u64();
  r.array(fifo_data_);
  fifo_wr_ = r.u32() & 3U;
  if (fifo_head_ >= fifo_done_.size() || fifo_count_ > fifo_done_.size())
    r.fail();
}

} // namespace tessera::md
