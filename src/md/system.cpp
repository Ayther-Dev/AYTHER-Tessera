#include "md/system.hpp"


namespace tessera::md {
namespace {
std::uint16_t dup(std::uint8_t v) {
  return static_cast<std::uint16_t>((v << 8) | v);
}
// Extra Z80 T-states of an access to the 68000 bus through the bank window.
constexpr unsigned kZ80RomWait = 3;
// How far the Z80 INT leads the vertical interrupt of the 68000, in master cycles.
constexpr std::uint64_t kZ80IntLead = 1620;
// YM2612 busy time after a data write, master cycles (probe ym-busy-N)
constexpr std::uint64_t kYmBusy = 1325;
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
  z80_.reset();
  z80_bank_ = 0;
  z80_master_ = 0;
  z80_int_end_ = 0;
  io_data_ = {0x00, 0x00, 0x00};
  io_ctrl_ = {0x00, 0x00, 0x00};
  frame_ = 0;
  current_line_ = 0;
  frame_start_line_ = 192;
  line_start_master_ = 0;
  cpu_.reset();
  refresh_due_ = 0;
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
  refresh_due_ = cpu_.getClock() + 130;
  z80_master_ = boot_master;
}

void System::update_irq() noexcept { cpu_.setIPL(vdp_.irq_level()); }

// DRAM refresh: gpgx takes 2 CPU cycles away from the 68000 once every 130
// cycles it executes (one stolen cycle count per 132 in all: 1.515 %, measured
// with cpu_drift_long over up to 21 million cycles, TP-104.3).
void System::run_cpu_until(moira::i64 target) {
  while (cpu_.getClock() < target) {
    if (refresh_due_ > target) {
      cpu_.executeUntil(target);
      break;
    }
    cpu_.executeUntil(refresh_due_);
    if (cpu_.getClock() >= refresh_due_) {
      cpu_.stall(2);
      refresh_due_ = cpu_.getClock() + 130;
    }
  }
}

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
    vdp_.set_line_start(line_start_master_);
    vdp_.begin_line(line, false);
    if (frame_ == 0) {
      vdp_.cancel_hint();
      vdp_.power_on_hint_counter();
    }
    update_irq();
    if (line == 0)
      renderer_.begin_frame(vdp_);
    if (line == 0)
      vdp_.set_odd_field(renderer_.field() == 1);
    if (index == 0 && frame_ != 0) {
      // The vertical interrupt comes vint_cycle cycles after the frame start
      // (gpgx), so a horizontal interrupt of that same line is served first.
      run_cpu_until(static_cast<moira::i64>(
          (line_start_master_ + vdp_.vint_cycle()) / kDivider68k));
      // The VDP holds the Z80 INT line for one scanline from the vertical
      // interrupt, whether or not the 68000 enables it (gpgx z80_int probe).
      vdp_.raise_vint();
      update_irq();
    }
    const std::uint64_t end_master = line_start_master_ + kCyclesPerLine;
    run_cpu_until(static_cast<moira::i64>(end_master / kDivider68k));
    if (index + 1 == lines) {
      // The Z80 INT of the next vertical interrupt starts kZ80IntLead master
      // cycles before the 68000 sees it (gpgx z80_int probe) and lasts one
      // scanline, whether or not the 68000 enables the interrupt.
      const std::uint64_t int_start =
          end_master + vdp_.vint_cycle() - kZ80IntLead;
      z80_sync(int_start);
      z80_int_end_ = int_start + kCyclesPerLine;
    }
    z80_sync(end_master);
    renderer_.render_line(vdp_, line);
    const auto flags = renderer_.take_sprite_flags();
    vdp_.raise_sprite_flags(flags.overflow, flags.collision);
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
    z80_sync_now();
    z80_wait();
    if (!z80_bus_granted())
      return open_bus(offset, word);
    const std::uint32_t a = offset & 0x7FFFU;
    std::uint8_t v;
    if (a < 0x4000)
      v = z80_ram_[a & 0x1FFFU];
    else if (a < 0x6000)
      v = ym_status(cpu_.getClock() * kDivider68k);
    else if (a < 0x7F00)
      v = 0xFF;
    else
      return open_bus(offset, word); // the Z80 side VDP ports
    return word ? dup(v) : v;
  }
  case Target::io: {
    const std::uint8_t v = io_read(offset | 1U);
    return word ? dup(v) : v;
  }
  case Target::z80_control: {
    // Bit 0 of the word at $A11100 is low once the bus is granted; the other
    // bits are the open bus (the word the CPU has prefetched).
    if (offset == 0x1100 || offset == 0x1101) {
      const std::uint16_t irc = cpu_.getIRC();
      const std::uint16_t busy = z80_bus_granted() ? 0U : 1U;
      if (word)
        return static_cast<std::uint16_t>((irc & 0xFEFFU) | (busy << 8));
      return offset == 0x1100
                 ? static_cast<std::uint16_t>(((irc >> 8) & 0xFEU) | busy)
                 : static_cast<std::uint16_t>(irc & 0xFFU);
    }
    if (offset == 0x1200 || offset == 0x1201)
      return open_bus(offset, word); // the reset register is write only
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

// The 68000 loses one cycle on every access to the Z80 area (gpgx: the copy
// of the sound driver of Streets of Rage 2, 8 KiB of byte writes, ends in the
// same frame only with exactly one; 0.7 from the first 40-access probe was
// short and made the title lag a frame behind from its frame 156).
void System::z80_wait() { cpu_.stall(1); }

void System::write(Target target, std::uint32_t offset, std::uint16_t value,
                   bool word) {
  switch (target) {
  case Target::z80_area: {
    z80_sync_now();
    z80_wait();
    if (!z80_bus_granted())
      break; // the area does not answer without the bus
    const std::uint32_t a = offset & 0x7FFFU;
    const auto v = word ? static_cast<std::uint8_t>(value >> 8) // high byte
                        : static_cast<std::uint8_t>(value);
    if (a < 0x4000)
      z80_ram_[a & 0x1FFFU] = v;
    else if (a < 0x6000)
      ym_write(a, word ? static_cast<std::uint8_t>(value) : v,
               cpu_.getClock() * kDivider68k, word);
    else if ((a & 0xFF00U) == 0x6000U)
      z80_bank_write(v);
    break;
  }
  case Target::io:
    io_write(offset | (word ? 1U : 0U), static_cast<std::uint8_t>(value));
    break;
  case Target::z80_control:
    if (offset == 0x1100) {
      z80_sync_now();
      const bool request = word ? (value & 0x0100U) != 0 : (value & 1U) != 0;
      if (request && z80_running())
        z80_.finish_instruction(); // the Z80 grants the bus between instructions
      if (!z80_running() && !request && z80_reset_released_)
        z80_master_ = cpu_.getClock() * kDivider68k; // resumes from now
      z80_bus_requested_ = request;
    } else if (offset == 0x1200) {
      z80_sync_now();
      const bool released = word ? (value & 0x0100U) != 0 : (value & 1U) != 0;
      if (!released)
        z80_.soft_reset();
      else if (!z80_reset_released_)
        z80_master_ = cpu_.getClock() * kDivider68k;
      z80_reset_released_ = released;
    }
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

// Runs the Z80 up to `master`. It advances only while it owns its bus (out of
// reset and not requested by the 68000); otherwise its clock follows the time.
void System::z80_sync(std::uint64_t master) {
  if (!z80_running()) {
    if (master > z80_master_)
      z80_master_ = master;
    return;
  }
  while (z80_master_ + kDividerZ80 <= master) {
    z80_.set_int(z80_master_ < z80_int_end_);
    z80_.tick();
    z80_master_ += kDividerZ80;
  }
}

// What the 68000 reads from the Z80 area when the Z80 does not answer: the
// word the CPU has already prefetched (gpgx measured, z80_map).
std::uint16_t System::open_bus(std::uint32_t offset, bool word) const noexcept {
  const std::uint16_t irc = cpu_.getIRC();
  if (word)
    return irc;
  return (offset & 1U) != 0 ? static_cast<std::uint16_t>(irc & 0xFFU)
                            : static_cast<std::uint16_t>(irc >> 8);
}

// The YM2612 busy flag (bit 7 of the status): set for a fixed time by a write
// to a data port, not by an address write. Measured on gpgx with ym-busy-N; the
// timers and everything else of the chip are TP-107.
std::uint8_t System::ym_status(std::uint64_t now) const noexcept {
  return now < ym_busy_until_ ? 0x80U : 0U;
}

void System::ym_write(std::uint32_t address, std::uint8_t, std::uint64_t now,
                      bool word) noexcept {
  if ((address & 1U) != 0 || word)
    ym_busy_until_ = now + kYmBusy;
}

void System::z80_bank_write(std::uint8_t value) noexcept {
  z80_bank_ = static_cast<std::uint16_t>(((z80_bank_ >> 1) |
                                           ((value & 1U) << 8)) &
                                          0x1FFU);
}

std::uint8_t System::z80_read(std::uint16_t address, unsigned &wait) {
  wait = 0;
  if (address < 0x4000)
    return z80_ram_[address & 0x1FFFU];
  if (address < 0x6000)
    return ym_status(z80_master_);
  if (address < 0x8000)
    return 0xFF;
  const std::uint32_t a =
      (std::uint32_t{z80_bank_} << 15) | (address & 0x7FFFU);
  if (classify(a) == Target::rom) {
    wait = kZ80RomWait;
    return bus_.read8(a);
  }
  return 0xFF; // gpgx: work RAM and the rest read back open
}

void System::z80_write(std::uint16_t address, std::uint8_t value,
                       unsigned &wait) {
  wait = 0;
  if (address < 0x4000) {
    z80_ram_[address & 0x1FFFU] = value;
  } else if (address < 0x6000) {
    ym_write(address, value, z80_master_, false); // the rest is TP-107
  } else if (address < 0x8000) {
    if ((address & 0xFF00U) == 0x6000U)
      z80_bank_write(value);
  } else {
    const std::uint32_t a =
        (std::uint32_t{z80_bank_} << 15) | (address & 0x7FFFU);
    const Target t = classify(a);
    if (t == Target::work_ram || t == Target::rom)
      bus_.write8(a, value);
  }
}

} // namespace tessera::md
