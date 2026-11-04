#include "md/vdp.hpp"

namespace tessera::md {

std::uint32_t Vdp::active_lines() const noexcept {
  return (regs_[1] & 0x08U) != 0 ? 240U : 224U;
}

void Vdp::write_control(std::uint16_t value) {
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
    regs_[(value >> 8) & 0x1FU] = static_cast<std::uint8_t>(value);
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
  switch (code_ & 0x0FU) {
  case 1: // VRAM write
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
  } else if (mode == 2) { // VRAM fill: the data word, then its high byte
    put(first_data);
    const auto byte = static_cast<std::uint8_t>(first_data >> 8);
    for (std::uint32_t i = 0; i < length; ++i) {
      vram_[address_ ^ 1U] = byte;
      address_ = static_cast<std::uint16_t>(address_ + regs_[15]);
    }
  } else { // VRAM copy
    auto from = static_cast<std::uint16_t>(source >> 1);
    for (std::uint32_t i = 0; i < length; ++i) {
      vram_[address_] = vram_[from];
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
  if (line_ >= active_lines())
    status |= 0x0008U; // vertical blank
  if (vint_flag_)
    status |= 0x0080U;
  return status;
}

std::uint16_t Vdp::read_hv(std::uint32_t line_cycle) const noexcept {
  // NTSC counts 0x00-0xEA, then continues at 0xE5 up to 0xFF; PAL has its own
  // jump. Both are provisional until compared with gpgx.
  std::uint32_t v = line_;
  if (standard_ == Standard::ntsc) {
    if (v > 0xEA)
      v = v - 0xEA + 0xE4;
  } else if (v > 0x102) {
    v = v - 0x102 + 0xCA;
  }
  const std::uint32_t h = (line_cycle * 0x100U / kCyclesPerLine) & 0xFFU;
  return static_cast<std::uint16_t>(((v & 0xFFU) << 8) | h);
}

void Vdp::begin_line(std::uint32_t line) {
  line_ = line;
  if (line == 0)
    hint_counter_ = regs_[10];
  if (line < active_lines()) {
    if (hint_counter_ == 0) {
      hint_flag_ = true;
      hint_counter_ = regs_[10];
    } else {
      --hint_counter_;
    }
  } else if (line == active_lines()) {
    vint_flag_ = true;
  }
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
