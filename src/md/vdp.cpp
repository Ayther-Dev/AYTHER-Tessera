#include "md/vdp.hpp"

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
    if (index <= 10 || (regs_[1] & 0x04U) != 0)
      regs_[index] = static_cast<std::uint8_t>(value);
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
    const std::uint32_t a = address_ & 0x3FFFU;
    const std::uint32_t index = a & 0x3FFEU;
    const auto word = static_cast<std::uint16_t>(
        (a & 1U) != 0 ? ((value >> 8) | (value << 8)) : value);
    vram_[index] = static_cast<std::uint8_t>(word >> 8);
    vram_[index + 1U] = static_cast<std::uint8_t>(word);
    address_ = static_cast<std::uint16_t>((a + regs_[15] + 1U) & 0x3FFFU);
    return;
  }
  switch (code_ & 0x0FU) {
  case 1:                       // VRAM write
    if ((address_ & 1U) != 0) { // odd address: bytes swapped
      vram_[address_] = static_cast<std::uint8_t>(value >> 8);
      vram_[address_ ^ 1U] = static_cast<std::uint8_t>(value);
    } else {
      vram_[address_] = static_cast<std::uint8_t>(value >> 8);
      vram_[address_ + 1U] = static_cast<std::uint8_t>(value);
    }
    break;
  case 3:
    cram_[(address_ >> 1) & 0x3FU] = value & 0x0EEEU;
    break;
  case 5:
    if (((address_ >> 1) & 0x7FU) < vsram_.size())
      vsram_[(address_ >> 1) & 0x7FU] = value & 0x07FFU;
    break;
  default:
    break;
  }
  address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
}

void Vdp::write_data(std::uint16_t value) {
  pending_second_ = false;
  if (dma_pending_) {
    dma_pending_ = false;
    run_dma(value);
    return;
  }
  put(value);
}

std::uint16_t Vdp::read_data() {
  pending_second_ = false;
  if (mode4()) {
    // Mode 4 VRAM read (gpgx vdp_68k_data_r_m4): the word of the even address,
    // then the address steps by register 15 + 1.
    const std::uint32_t a = address_ & 0x3FFFU;
    const auto value =
        static_cast<std::uint16_t>((vram_[a & 0x3FFEU] << 8) | vram_[a | 1U]);
    address_ = static_cast<std::uint16_t>((a + regs_[15] + 1U) & 0x3FFFU);
    return value;
  }
  std::uint16_t value = 0;
  switch (code_ & 0x0FU) {
  case 0:
    value = static_cast<std::uint16_t>((vram_[address_ & 0xFFFEU] << 8) |
                                       vram_[(address_ & 0xFFFEU) + 1U]);
    break;
  case 8:
    value = cram_[(address_ >> 1) & 0x3FU];
    break;
  case 4:
    if (((address_ >> 1) & 0x7FU) < vsram_.size())
      value = vsram_[(address_ >> 1) & 0x7FU];
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
    for (std::uint32_t i = 0; i < length; ++i, at += 2)
      put(dma_reader_ ? dma_reader_(at) : std::uint16_t{0xFFFF});
    cpu_stall_ += dma_cycles(length);
    return;
  }
  // Fill: one slot per byte; copy: two slots per byte (read and write).
  busy_until_ = now_ + dma_cycles(mode == 2 ? length : 2ULL * length);
  if (mode == 2) { // VRAM fill: the data word, then its high byte
    put(first_data);
    const auto byte = static_cast<std::uint8_t>(first_data >> 8);
    for (std::uint32_t i = 0; i < length; ++i) {
      vram_[address_ ^ 1U] = byte;
      address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
    }
  } else { // VRAM copy
    auto from = static_cast<std::uint16_t>(source >> 1);
    for (std::uint32_t i = 0; i < length; ++i) {
      vram_[address_ ^ 1U] = vram_[from ^ 1U];
      ++from;
      address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
    }
  }
}

std::uint16_t Vdp::read_status() {
  pending_second_ = false;
  std::uint16_t status = 0x3400U | 0x0200U; // fixed bits, FIFO empty
  if (standard_ == Standard::pal)
    status |= 0x0001U;
  if (display_blank())
    status |= 0x0008U; // blanking: vertical blank or display disabled
  if (now_ < busy_until_)
    status |= 0x0002U; // DMA fill or copy in progress
  if (vint_flag_)
    status |= 0x0080U;
  if (sprite_overflow_)
    status |= 0x0040U;
  if (sprite_collision_)
    status |= 0x0020U;
  sprite_overflow_ = sprite_collision_ = false;
  return status;
}

bool Vdp::display_blank() const noexcept {
  return line_ >= active_lines() || (regs_[1] & 0x40U) == 0;
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
  if (line <= active_lines()) {
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

} // namespace tessera::md
