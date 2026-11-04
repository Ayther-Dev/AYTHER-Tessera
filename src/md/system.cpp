#include "md/system.hpp"

namespace tessera::md {
namespace {
std::uint16_t dup(std::uint8_t v) {
  return static_cast<std::uint16_t>((v << 8) | v);
}
} // namespace

System::System(std::vector<std::uint8_t> rom, const MachineConfig &config)
    : config_(config), sram_(read_sram_header(rom)),
      bus_(std::move(rom), *this, config.mapper, &sram_), cpu_(bus_),
      vdp_(config.standard), tmss_(config.tmss) {
  vdp_.set_dma_reader([this](std::uint32_t address) -> std::uint16_t {
    return bus_.read16(address);
  });
  cpu_.set_interrupt_hook([this](std::uint8_t level) {
    vdp_.acknowledge(level);
    update_irq();
  });
}

std::uint8_t System::io_version() const noexcept {
  std::uint8_t v = 0x20; // no expansion unit attached
  if (config_.region != Region::japan)
    v |= 0x80; // overseas
  if (config_.standard == Standard::pal)
    v |= 0x40;
  return v;
}

void System::reset() {
  vdp_ = Vdp(config_.standard);
  vdp_.set_dma_reader([this](std::uint32_t address) -> std::uint16_t {
    return bus_.read16(address);
  });
  tmss_ = Tmss(config_.tmss);
  for (auto &byte : bus_.work_ram())
    byte = 0;
  z80_ram_.fill(0);
  z80_bus_requested_ = false;
  z80_reset_released_ = false;
  io_data_ = {0x00, 0x00, 0x00};
  io_ctrl_ = {0x00, 0x00, 0x00};
  frame_ = 0;
  current_line_ = 0;
  frame_start_line_ = 192;
  line_start_master_ = 0;
  cpu_.reset();
  // On a hard reset the 68000 starts at a fixed point of the frame, not at its
  // beginning: gpgx (genesis.c, gen_reset) measured on hardware that the first
  // HV counter read is $9F21 (NTSC) or $8421 (PAL), i.e. line 159 (132) and
  // 1004 master cycles. The CPU clock starts there and the lines before it run
  // without the CPU.
  // (the frame starts at line 192, so that is 229 lines into it, 253 in PAL)
  const std::uint64_t boot_lines =
      config_.standard == Standard::pal ? 253U : 229U;
  const std::uint64_t boot_master = boot_lines * kCyclesPerLine + 1004U;
  // 68000 cycles of the reset sequence that gpgx counts before the first
  // instruction: 31 reproduces the $9F21 read (30 to 32 all do).
  constexpr moira::i64 kBootCpuCycles = 31;
  cpu_.setClock(
      static_cast<moira::i64>(boot_master / kDivider68k + kBootCpuCycles));
}

void System::update_irq() noexcept { cpu_.setIPL(vdp_.irq_level()); }

void System::run_frame() {
  const std::uint32_t lines = lines_per_frame(config_.standard);
  const std::uint64_t frame_master =
      frame_ * cycles_per_frame(config_.standard);
  // A frame starts at the first line of the vertical blanking (gpgx); the
  // first one at line 192, the height of Mode 4 in which the VDP wakes up.
  const std::uint32_t first_line = frame_start_line_ % lines;
  for (std::uint32_t index = 0; index < lines; ++index) {
    const std::uint32_t line = (first_line + index) % lines;
    current_line_ = line;
    line_start_master_ = frame_master + std::uint64_t{index} * kCyclesPerLine;
    vdp_.begin_line(line, false);
    if (frame_ == 0) {
      vdp_.cancel_hint();
      vdp_.power_on_hint_counter();
    }
    update_irq();
    if (line == 0)
      renderer_.begin_frame(vdp_);
    if (index == 0 && frame_ != 0) {
      // The vertical interrupt comes vint_cycle cycles after the frame start
      // (gpgx), so a horizontal interrupt of that same line is served first.
      cpu_.executeUntil(static_cast<moira::i64>(
          (line_start_master_ + vdp_.vint_cycle()) / kDivider68k));
      vdp_.raise_vint();
      update_irq();
    }
    const std::uint64_t end_master = line_start_master_ + kCyclesPerLine;
    cpu_.executeUntil(static_cast<moira::i64>(end_master / kDivider68k));
    renderer_.render_line(vdp_, line);
  }
  ++frame_;
  frame_start_line_ = vdp_.frame_start_line();
}

void System::set_pad(unsigned port, std::uint8_t pressed) noexcept {
  if (port < pads_.size())
    pads_[port] = pressed;
}

// Pad port: 3 button protocol. TH (bit 6) selects the half of the buttons.
std::uint8_t System::io_read(std::uint32_t offset) const noexcept {
  switch (offset) {
  case 0x01:
    return io_version();
  case 0x03:
  case 0x05: {
    const unsigned port = (offset - 3) / 2;
    const std::uint8_t out = io_data_[port];
    const std::uint8_t ctrl = io_ctrl_[port];
    const std::uint8_t pressed = pads_[port];
    const bool th = (out & 0x40U) != 0 || (ctrl & 0x40U) == 0;
    std::uint8_t in;
    if (th) { // C B right left down up
      in = static_cast<std::uint8_t>(
          ((pressed & kPadUp) ? 0 : 0x01) | ((pressed & kPadDown) ? 0 : 0x02) |
          ((pressed & kPadLeft) ? 0 : 0x04) |
          ((pressed & kPadRight) ? 0 : 0x08) | ((pressed & kPadB) ? 0 : 0x10) |
          ((pressed & kPadC) ? 0 : 0x20) | 0x40);
    } else { // start A 0 0 down up
      in = static_cast<std::uint8_t>(
          ((pressed & kPadUp) ? 0 : 0x01) | ((pressed & kPadDown) ? 0 : 0x02) |
          ((pressed & kPadA) ? 0 : 0x10) | ((pressed & kPadStart) ? 0 : 0x20));
    }
    return static_cast<std::uint8_t>((out & ctrl) | (in & ~ctrl));
  }
  case 0x07:
    return static_cast<std::uint8_t>((io_data_[2] & io_ctrl_[2]) |
                                     (0xFFU & ~io_ctrl_[2]));
  case 0x09:
  case 0x0B:
  case 0x0D:
    return io_ctrl_[(offset - 9) / 2];
  default:
    return 0xFF;
  }
}

// Level of the TH pin of port 2 as the VDP sees it: the data bit when the pin
// is an output, high (pulled up) when it is an input.
bool System::th2() const noexcept {
  return (io_ctrl_[1] & 0x40U) == 0 || (io_data_[1] & 0x40U) != 0;
}

std::uint32_t System::line_cycle() const noexcept {
  const std::uint64_t master = cpu_.getClock() * kDivider68k;
  return master > line_start_master_
             ? static_cast<std::uint32_t>(master - line_start_master_)
             : 0U;
}

void System::io_write(std::uint32_t offset, std::uint8_t value) noexcept {
  const bool th_before = th2();
  switch (offset) {
  case 0x03:
  case 0x05:
  case 0x07:
    io_data_[(offset - 3) / 2] = value;
    break;
  case 0x09:
  case 0x0B:
  case 0x0D:
    io_ctrl_[(offset - 9) / 2] = value;
    break;
  default:
    break;
  }
  if (th_before && !th2()) // TH falling on port 2 latches the HV counter
    vdp_.latch_hv(line_cycle());
}

std::uint16_t System::read(Target target, std::uint32_t offset, bool word) {
  switch (target) {
  case Target::z80_area: {
    // The Z80 side is not emulated yet: RAM is plain storage, the sound chip
    // ports read back zero (YM2612 status: not busy, no timer flag).
    if (offset < 0x4000)
      return word ? dup(z80_ram_[offset & 0x1FFFU])
                  : z80_ram_[offset & 0x1FFFU];
    return 0;
  }
  case Target::io: {
    const std::uint8_t v = io_read(offset | 1U);
    return word ? dup(v) : v;
  }
  case Target::z80_control: {
    if (offset == 0x1100) { // bus request: bit 0 low = bus granted
      const std::uint8_t high = z80_bus_requested_ ? 0x00 : 0x01;
      return word ? static_cast<std::uint16_t>(high << 8) : high;
    }
    if (offset == 0x1200)
      return word ? (z80_reset_released_ ? 0x0100 : 0x0000)
                  : (z80_reset_released_ ? 0x01 : 0x00);
    return word ? 0xFFFFU : 0xFFU;
  }
  case Target::vdp: {
    vdp_.set_time(cpu_.getClock() * kDivider68k);
    if (offset < 4) {
      const std::uint16_t v = vdp_.read_data();
      return word ? v : static_cast<std::uint8_t>((offset & 1U) ? v : v >> 8);
    }
    if (offset < 8) {
      const std::uint16_t v = vdp_.read_status();
      return word ? v : static_cast<std::uint8_t>((offset & 1U) ? v : v >> 8);
    }
    if (offset < 0x10) {
      const std::uint16_t v = vdp_.read_hv(line_cycle());
      return word ? v : static_cast<std::uint8_t>((offset & 1U) ? v : v >> 8);
    }
    return word ? 0xFFFFU : 0xFFU;
  }
  default:
    return word ? 0xFFFFU : 0xFFU;
  }
}

void System::write(Target target, std::uint32_t offset, std::uint16_t value,
                   bool word) {
  switch (target) {
  case Target::z80_area:
    if (offset < 0x4000) // a word write stores its high byte
      z80_ram_[offset & 0x1FFFU] = word ? static_cast<std::uint8_t>(value >> 8)
                                        : static_cast<std::uint8_t>(value);
    break;
  case Target::io:
    io_write(offset | (word ? 1U : 0U), static_cast<std::uint8_t>(value));
    break;
  case Target::z80_control:
    if (offset == 0x1100)
      z80_bus_requested_ = word ? (value & 0x0100U) != 0 : (value & 1U) != 0;
    else if (offset == 0x1200)
      z80_reset_released_ = word ? (value & 0x0100U) != 0 : (value & 1U) != 0;
    break;
  case Target::vdp: {
    const std::uint16_t v =
        word ? value : dup(static_cast<std::uint8_t>(value));
    vdp_.set_time(cpu_.getClock() * kDivider68k);
    if (offset < 4)
      vdp_.write_data(v);
    else if (offset < 8) {
      vdp_.write_control(v);
      update_irq(); // a register write can enable a pending interrupt
    }
    if (const auto stall = vdp_.take_cpu_stall(); stall != 0)
      cpu_.stall(static_cast<int>(stall / kDivider68k)); // DMA owns the bus
    break;
  }
  case Target::tmss:
    tmss_.write(offset, value, word);
    break;
  default:
    break;
  }
}

} // namespace tessera::md
