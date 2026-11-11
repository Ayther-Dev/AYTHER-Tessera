#include "md/system.hpp"

namespace tessera::md {
namespace {
std::uint16_t dup(std::uint8_t v) {
  return static_cast<std::uint16_t>((v << 8) | v);
}
// Extra Z80 T-states of an access to the 68000 bus through the bank window.
constexpr unsigned kZ80RomWait = 3;
constexpr unsigned kZ80VdpWait = 3;
// 68000 time, in master cycles, that one access of the Z80 to the 68000 bus
// takes (bank window read or write, VDP window write). Measured, TP-209.2:
// z80_steal, the 68000 counting a 30-cycle loop over the 224 active lines
// while the Z80 reads the ROM through the window every 28 + 4n T-states loses
// 71.7-72.3 master cycles per access for n = 1-16 (69.4 for n = 0); reads and
// writes cost the same and a read of the Z80's own RAM costs nothing. The Z80
// side is unchanged (z80_bank_rate: 50 T-states per pass on both cores).
// Known residue: when the Z80 period is a multiple of 105 master cycles every
// access lands at the same point of the 68000 loop and gpgx takes 10 cycles at
// all seven phases (z80_steal -f), where this rounding takes 11 at two of them.
constexpr std::uint64_t kZ80BusSteal = 72;
// When the 68000 gives the bus back or releases the reset line (the same
// stamps, 70 phases), the Z80 resumes on its own clock: at the
// first Z80 cycle edge (a multiple of 15 master cycles, frames are whole Z80
// cycles) at or after the write plus 51 master cycles (gpgx fm_psg_z80 with
// 0 to 69 NOPs before the release and 0 to 39 before the whole program: 110
// release phases, 51 is the only lead that stamps every Z80 write the same,
// TP-209.2). A fixed 67 matched one phase only.
constexpr std::uint64_t kZ80ResumeLead = 51;
std::uint64_t z80_resume_at(std::uint64_t master) noexcept {
  const std::uint64_t at = master + kZ80ResumeLead;
  return (at + kDividerZ80 - 1) / kDividerZ80 * kDividerZ80;
}
// A six-button pad that sees no change of TH for this long starts its sequence
// again (gpgx pad_latch-2: the ID of the second low half survives a pause of up
// to 12 540 68000 cycles and is gone from 12 910; the cycles in between depend
// on the scanline phase, so the middle is taken).
constexpr std::uint64_t kPad6Timeout = 12'700ULL * 7ULL;
// Cycles between the control write that turns TH into an input and the moment
// the pad sees the pull-up (gpgx pad_latch-5: a read 20 cycles after the write
// still sees the old half, one 24 cycles after sees the new one).
constexpr std::uint64_t kPad6PullUp = 24ULL * 7ULL;
// How far the Z80 INT leads the vertical interrupt of the 68000, in master
// cycles.
constexpr std::uint64_t kZ80IntLead = 1620;
// Master cycle of the line at which the VDP line is drawn, and at which its
// pixel 0 would be drawn when the display turns on in the middle of the line
// (TP-209.1, see run_frame). The origins were measured again with the
// refresh model of TP-209.2, which no longer runs the 68000 one refresh
// stall (14 master cycles) early: a display turned on in the middle of the
// line, moved in 2 cycle steps over 62 phases, lands on the gpgx pixel at
// all of them only with 688 (H32) and 718 (H40).
constexpr std::uint64_t kRenderCycle = 800;
constexpr std::uint32_t kPixelOriginH32 = 688;
constexpr std::uint32_t kPixelOriginH40 = 718;
// YM2612 busy flag after a data write (TP-209.2, ym-busy-N with the write
// moved over all 6 phases of the YM clock and over frames 0 to 5, 1482 cases):
// it drops at a YM clock edge (every 42 master cycles, MCLK / 7 / 6), the
// first at or after the write plus kYmBusy. The edges restart with each frame
// (a frame is not a whole number of YM clocks) at kYmEdge cycles from its
// start. A flat 1325 cycles matched 1023 of the 1152 frame 0 cases.
constexpr std::uint64_t kYmBusy = 1287;
constexpr std::uint64_t kYmClock = 42;
constexpr std::uint64_t kYmEdge = 28;
// The Z80 sees the flag drop later than the 68000 does: polling it from the
// Z80 (fm-busy-z80-N probes, TP-107.7) puts the end between 1336 and about
// 1395 master cycles after the write, the 68000 probes (ym-busy-N) at no more
// than 1330. One Z80 clock more on its writes satisfies both.
constexpr std::uint64_t kZ80YmBusyExtra = 15;
} // namespace

namespace {
std::uint64_t fnv1a(std::span<const std::uint8_t> bytes) noexcept {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto b : bytes) {
    hash ^= b;
    hash *= 1099511628211ULL;
  }
  return hash;
}
constexpr std::uint32_t kStateMagic = 0x53444D54; // "TMDS"
constexpr std::uint32_t kStateVersion = 1;
constexpr std::uint32_t kStateEnd = 0x444E4553; // "SEND"
} // namespace

namespace {
/// CRC-32 (IEEE 802.3, reflected) of the ROM image.
std::uint32_t crc32(const std::vector<std::uint8_t> &data) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const std::uint8_t byte : data) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}
} // namespace

System::System(std::vector<std::uint8_t> rom, const MachineConfig &config)
    : config_(config), rom_hash_(fnv1a(rom)), rom_crc32_(crc32(rom)),
      rom_bytes_(static_cast<std::uint32_t>(rom.size())),
      sram_(read_sram_header(rom)),
      bus_(std::move(rom), *this, config.mapper, &sram_), cpu_(bus_),
      vdp_(config.standard), tmss_(config.tmss), mixer_(config.standard) {
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

namespace {
// First DRAM refresh request after a hard reset, in 68000 cycles from the
// first instruction (TP-209.2, measured: the first stall falls where a
// request 62 to 64 cycles in would put it, with three instruction mixes).
constexpr moira::i64 kFirstRefresh = 64;
} // namespace

void System::reset() {
  vdp_ = Vdp(config_.standard);
  vdp_.set_dma_reader([this](std::uint32_t address) -> std::uint16_t {
    return bus_.read16(address);
  });
  tmss_ = Tmss(config_.tmss);
  renderer_.reset();
  for (auto &byte : bus_.work_ram())
    byte = 0;
  z80_ram_.fill(0);
  z80_bus_requested_ = false;
  z80_reset_released_ = false;
  z80_.reset();
  z80_bank_ = 0;
  ym_.reset();
  psg_.reset();
  mixer_.reset();
  ym_addr_ = 0;
  ym_busy_until_ = 0;
  audio_log_.clear();
  audio_dropped_ = 0;
  log_origin_ = 0;
  z80_master_ = 0;
  z80_int_end_ = 0;
  io_data_ = {0x00, 0x00, 0x00};
  io_ctrl_ = {0x00, 0x00, 0x00};
  pad_port_ = {};
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
  refresh_due_ = cpu_.getClock() + kFirstRefresh;
  z80_master_ = boot_master;
}

namespace {
constexpr std::uint64_t kSoftResetHold = 466179; // master cycles
} // namespace

void System::soft_reset() {
  const auto clock = cpu_.getClock();
  z80_bus_requested_ = false;
  z80_reset_released_ = false;
  z80_.soft_reset();
  z80_bank_ = 0;
  ym_.soft_reset();
  ym_addr_ = 0;
  ym_busy_until_ = 0;
  cpu_.reset();
  // The reset sequence takes the same 68000 cycles as after a hard reset;
  // before it the 68000 stays halted (TP-110.4).
  // The hold counts from the start of the frame, not from where the last
  // instruction left the clock (measured: the restart point does not move
  // with the reset frame).
  const std::uint64_t restart =
      frame_ * cycles_per_frame(config_.standard) + kSoftResetHold;
  const auto start = static_cast<moira::i64>(
      (restart + kDivider68k - 1) / kDivider68k); // first CPU cycle at or after
  cpu_.setClock(std::max(start, clock) + 31);
  update_irq();
}

void System::update_irq() noexcept { cpu_.setIPL(vdp_.irq_level()); }

void System::raise_sprite_flags() noexcept {
  const auto flags = renderer_.take_sprite_flags();
  vdp_.raise_sprite_flags(flags.overflow, flags.collision);
}

namespace {
// DRAM refresh of the 68000 (TP-209.2, measured on gpgx as a black box with
// the master cycle stamps of PSG writes, see docs/clean-room-register.md):
// a refresh is requested 128 CPU cycles after the previous one started. A
// request that is due at an instruction boundary costs the 68000 2 cycles
// there and the next one comes 128 cycles after that boundary. A register
// count shift (ASd/LSd/ROXd/ROd Dx,Dy) leaves the bus free during its first
// 2n + 2 cycles (n = Dx & 63, n >= 1): a request that falls in them is served
// at no cost and the next one comes 128 cycles after it. DIVU and DIVS do the
// same during their first L - 4 cycles (L = the whole instruction, at most
// 128), except when the quotient overflows.
constexpr moira::i64 kRefreshPeriod = 128;
constexpr int kRefreshCost = 2;

enum class FreeBus : std::uint8_t { none, shift, divide };

/// Kind of free bus window of the instruction about to run (`opcode`); for a
/// shift, `window` gets its length.
FreeBus refresh_free_kind(const moira::Moira &cpu, std::uint16_t opcode,
                          int &window) {
  // 1110 ccc d ss 1 tt rrr with ss != 11: count in data register ccc
  if ((opcode & 0xF020U) == 0xE020U && (opcode & 0x00C0U) != 0x00C0U) {
    const auto n = static_cast<int>(cpu.getD((opcode >> 9) & 7U) & 0x3FU);
    window = 2 * n + 2;
    return n == 0 ? FreeBus::none : FreeBus::shift;
  }
  // 1000 rrr 011 mmmrrr (DIVU) / 1000 rrr 111 mmmrrr (DIVS)
  if ((opcode & 0xF0C0U) == 0x80C0U)
    return FreeBus::divide;
  return FreeBus::none;
}
} // namespace

void System::run_cpu_until(moira::i64 target) {
  while (cpu_.getClock() < target) {
    const auto clock = cpu_.getClock();
    if (clock >= refresh_due_) {
      cpu_.stall(kRefreshCost);
      refresh_due_ = clock + kRefreshPeriod;
      continue;
    }
    // An instruction that starts more than 128 cycles before the request
    // cannot absorb it (the longest free window is 128 cycles).
    const auto far = refresh_due_ - kRefreshPeriod + 1;
    if (clock < far) {
      cpu_.executeUntil(std::min(target, far));
      continue;
    }
    const auto pc = cpu_.getPC0();
    int window = 0;
    const FreeBus kind = refresh_free_kind(cpu_, cpu_.getIRD(), window);
    cpu_.execute();
    if (kind == FreeBus::none)
      continue;
    // Only when the instruction itself ran to its end: not an interrupt taken
    // in its place, nor a division by zero (the PC moves on by the opcode and
    // its extension words, at most 8 bytes).
    const auto next = cpu_.getPC0();
    if (next <= pc || next > pc + 8)
      continue;
    if (kind == FreeBus::divide) {
      if ((cpu_.getSR() & 0x0002U) != 0) // V: the quotient overflowed
        continue;
      window = static_cast<int>(
          std::min<moira::i64>(cpu_.getClock() - clock - 4, kRefreshPeriod));
    }
    while (refresh_due_ >= clock && refresh_due_ < clock + window)
      refresh_due_ += kRefreshPeriod;
  }
}

void System::run_frame() {
  const std::uint32_t lines = lines_per_frame(config_.standard);
  const std::uint64_t frame_master =
      frame_ * cycles_per_frame(config_.standard);
  // A frame starts at the first line of the vertical blanking (gpgx); the
  // first one at line 192, the height of Mode 4 in which the VDP wakes up.
  const std::uint32_t first_line = frame_start_line_ % lines;
  log_origin_ = frame_master;
  audio_log_.clear();
  audio_dropped_ = 0;
  ym_.clear_samples();
  psg_.clear_samples();
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
    if (index == 0) {
      // GEOMETRY_PENDING counts the size changes of this frame, from its
      // first line on (measured, raster_bands 18: a change before line 0
      // still sets it).
      vdp_.clear_geometry_changed();
      vdp_.begin_frame_tracking();
    }
    if (line == 0)
      renderer_.begin_frame(vdp_);
    if (line == 0)
      vdp_.set_odd_field(renderer_.field() == 1);
    renderer_.latch_line(vdp_, line);
    vdp_.begin_line_vram();
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
    // The line is drawn kRenderCycle master cycles after its start, not at its
    // end: a CRAM or register write before that point shows on this line, one
    // after it on the next (measured, TP-209.1: modes_sweep CRAM sweep with
    // the display off and on, 760-860 all fit the 413 synthetic ROMs, the CRAM
    // sweep narrows it to 782-818).
    line_drawn_ = false;
    display_on_cycle_.reset();
    const std::uint64_t draw_master = line_start_master_ + kRenderCycle;
    run_cpu_until(static_cast<moira::i64>(draw_master / kDivider68k));
    z80_sync(draw_master);
    renderer_.render_line(vdp_, line);
    raise_sprite_flags();
    line_drawn_ = true;
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
    if (display_on_cycle_ && (vdp_.reg(1) & 0x40U) != 0) {
      // The display turned on after the line was drawn: the pixels from the
      // write on show the picture, the ones before keep the backdrop. Pixel x
      // is drawn at kPixelOrigin + x * dot (H32 10, H40 8 master cycles;
      // measured, TP-209.1: modes_sweep 0, 1, 6, 9, 10, 16, raster_bands 0).
      const bool h40 = (vdp_.reg(12) & 0x81U) != 0;
      const std::uint32_t origin = h40 ? kPixelOriginH40 : kPixelOriginH32;
      const std::uint32_t dot = h40 ? 8U : 10U;
      const std::uint32_t at = *display_on_cycle_;
      const std::uint32_t first =
          at <= origin ? 0U : (at - origin + dot - 1U) / dot;
      renderer_.redraw_line_from(vdp_, line, first);
      raise_sprite_flags();
    }
    line_drawn_ = false;
  }
  ym_.run_to(frame_master + cycles_per_frame(config_.standard));
  psg_.run_to(frame_master + cycles_per_frame(config_.standard));
  mixer_.push(ym_.samples(), psg_.samples(), ym_.raw_samples(),
              psg_.raw_samples());
  for (const auto &active : cheats_) {
    if (!cheat_targets_ram(active.cheat))
      continue;
    auto ram = bus_.work_ram();
    const std::size_t at = active.cheat.address & 0xFFFFU;
    ram[at] = static_cast<std::uint8_t>(active.cheat.data >> 8);
    ram[at + 1] = static_cast<std::uint8_t>(active.cheat.data);
  }
  if (audio_events_on_) {
    const std::uint64_t cycles = cycles_per_frame(config_.standard);
    AudioEvent event{};
    event.t_global = frame_master + cycles;
    event.t_frame = static_cast<std::uint32_t>(frame_);
    event.t_cycles = static_cast<std::uint32_t>(cycles);
    event.type = static_cast<std::uint8_t>(AudioEventType::frame);
    event.channel = kAudioNoChannel;
    event.schema = kAudioEventSchema;
    audio_events_->push(event);
  }
  ++frame_;
  frame_start_line_ = vdp_.frame_start_line();
}

std::uint32_t System::add_cheat(std::string_view code) {
  const auto cheat = decode_cheat(code);
  if (!cheat || next_cheat_id_ == 0)
    return 0;
  const std::uint32_t id = next_cheat_id_++;
  cheats_.push_back({id, *cheat});
  refresh_cheats();
  return id;
}

bool System::remove_cheat(std::uint32_t id) {
  const auto at =
      std::find_if(cheats_.begin(), cheats_.end(),
                   [id](const ActiveCheat &c) { return c.id == id; });
  if (at == cheats_.end())
    return false;
  cheats_.erase(at);
  refresh_cheats();
  return true;
}

void System::clear_cheats() {
  cheats_.clear();
  refresh_cheats();
}

void System::refresh_cheats() {
  std::vector<Bus::RomPatch> patches;
  for (const auto &active : cheats_)
    if (!cheat_targets_ram(active.cheat))
      patches.push_back({active.cheat.address, active.cheat.data});
  bus_.set_rom_patches(std::move(patches));
}

void System::set_pad(unsigned port, std::uint16_t pressed) noexcept {
  if (port < pads_.size())
    pads_[port] = pressed;
}

// Pad port: 3 button protocol. TH (bit 6) selects the half of the buttons.
std::uint8_t System::io_read(std::uint32_t offset) noexcept {
  switch (offset) {
  case 0x01:
    return io_version();
  case 0x03:
  case 0x05: {
    const unsigned port = (offset - 3) / 2;
    const std::uint8_t out = io_data_[port];
    const std::uint8_t ctrl = io_ctrl_[port];
    const std::uint16_t pressed = pads_[port];
    const bool six = config_.pads[port] == PadType::six_button;
    pad_settle(port, cpu_.getClock() * kDivider68k);
    const bool th = pad_port_[port].th;
    const unsigned phase = six ? pad_port_[port].phase : 0;
    std::uint8_t in;
    if (six && th && phase == 3) { // C B mode X Y Z
      in = static_cast<std::uint8_t>(
          ((pressed & kPadZ) ? 0 : 0x01) | ((pressed & kPadY) ? 0 : 0x02) |
          ((pressed & kPadX) ? 0 : 0x04) | ((pressed & kPadMode) ? 0 : 0x08) |
          ((pressed & kPadB) ? 0 : 0x10) | ((pressed & kPadC) ? 0 : 0x20) |
          0x40);
    } else if (six && !th && phase >= 2 && phase <= 3) {
      // start A and, in place of the directions, the ID of the six-button pad
      // (0000) in the second low half and 1111 in the third
      in = static_cast<std::uint8_t>((phase == 2 ? 0x00 : 0x0F) |
                                     ((pressed & kPadA) ? 0 : 0x10) |
                                     ((pressed & kPadStart) ? 0 : 0x20));
    } else if (th) { // C B right left down up
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
    in |= 0x40; // TH as an input reads high (pull-up), whatever the phase
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

// What a six-button pad sees of TH after a write (TP-108.2). With TH an output
// it follows the data register; with TH an input the pull-up holds it high.
// A rise of an output TH is one step of the sequence; every change restarts
// the time the pad waits before it forgets the sequence. Changing the control
// register to make TH an input does not reach the pad (gpgx pad_latch-4).
void System::pad_settle(unsigned port, std::uint64_t now) noexcept {
  auto &pad = pad_port_[port];
  if (pad.pull_up_at != 0 && now >= pad.pull_up_at) {
    if (!pad.th) {
      pad.th = true; // pulled up: the sequence does not advance
      pad.edge_at = pad.pull_up_at;
    }
    pad.pull_up_at = 0;
  }
  if (now - pad.edge_at > kPad6Timeout)
    pad.phase = 0; // idle for too long: the sequence restarts
}

void System::pad_notify(unsigned port, bool control_write,
                        std::uint64_t now) noexcept {
  pad_settle(port, now);
  auto &pad = pad_port_[port];
  const bool output = (io_ctrl_[port] & 0x40U) != 0;
  pad.pull_up_at = 0;
  if (control_write && !output) {
    if (!pad.th)
      pad.pull_up_at = now + kPad6PullUp;
    return;
  }
  const bool th = output ? (io_data_[port] & 0x40U) != 0 : true;
  if (th == pad.th)
    return;
  if (th && output && pad.phase < 4)
    ++pad.phase;
  pad.th = th;
  pad.edge_at = now;
}

void System::io_write(std::uint32_t offset, std::uint8_t value) noexcept {
  const bool th_before = th2();
  const std::uint64_t now = cpu_.getClock() * kDivider68k;
  switch (offset) {
  case 0x03:
  case 0x05:
    io_data_[(offset - 3) / 2] = value;
    pad_notify((offset - 3) / 2, false, now);
    break;
  case 0x07:
    io_data_[2] = value;
    break;
  case 0x09:
  case 0x0B:
    io_ctrl_[(offset - 9) / 2] = value;
    pad_notify((offset - 9) / 2, true, now);
    break;
  case 0x0D:
    io_ctrl_[2] = value;
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
    if (!z80_bus_granted())
      return open_bus(offset, word);
    const std::uint64_t at = cpu_.getClock() * kDivider68k; // before the wait
    z80_wait();
    const std::uint32_t a = offset & 0x7FFFU;
    std::uint8_t v;
    if (a < 0x4000)
      v = z80_ram_[a & 0x1FFFU];
    else if (a < 0x6000)
      v = ym_status(at);
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
      // Bits 15-10 of the status are the open bus: the word the CPU has
      // prefetched, as for $A11100 (measured on gpgx, TP-110.4: the bits
      // follow the instruction after the read).
      const std::uint16_t v = static_cast<std::uint16_t>(
          (vdp_.read_status() & 0x03FFU) | (cpu_.getIRC() & 0xFC00U));
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

// The 68000 loses one cycle on every access to the Z80 area while it holds
// the Z80 bus; without the bus (Z80 running or in reset) the access is open
// and costs nothing more (TP-209.2, HV probe: 8.19 cycles per move.b on gpgx
// either way, 9.20 with the bus). The cycle itself was measured with the copy
// of the sound driver of Streets of Rage 2 (8 KiB of byte writes end in the
// same frame only with exactly one; 0.7 from the first 40-access probe was
// short and made the title lag a frame behind from its frame 156).
void System::z80_wait() { cpu_.stall(1); }

void System::write(Target target, std::uint32_t offset, std::uint16_t value,
                   bool word) {
  switch (target) {
  case Target::z80_area: {
    z80_sync_now();
    if (!z80_bus_granted())
      break; // the area does not answer without the bus
    const std::uint64_t at = cpu_.getClock() * kDivider68k; // before the wait
    z80_wait();
    const std::uint32_t a = offset & 0x7FFFU;
    const auto v = word ? static_cast<std::uint8_t>(value >> 8) // high byte
                        : static_cast<std::uint8_t>(value);
    if (a < 0x4000)
      z80_ram_[a & 0x1FFFU] = v;
    else if (a < 0x6000)
      ym_write(a, v, at, word);
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
        z80_.finish_instruction(); // the Z80 grants the bus between
                                   // instructions
      if (!z80_running() && !request && z80_reset_released_)
        z80_master_ = z80_resume_at(cpu_.getClock() * kDivider68k);
      z80_bus_requested_ = request;
    } else if (offset == 0x1200) {
      z80_sync_now();
      const bool released = word ? (value & 0x0100U) != 0 : (value & 1U) != 0;
      if (!released)
        z80_.soft_reset();
      else if (!z80_reset_released_)
        z80_master_ = z80_resume_at(cpu_.getClock() * kDivider68k);
      z80_reset_released_ = released;
    }
    break;
  case Target::vdp: {
    const std::uint16_t v =
        word ? value : dup(static_cast<std::uint8_t>(value));
    vdp_.set_time(cpu_.getClock() * kDivider68k);
    if (offset >= 0x10 && offset < 0x18) {
      // The PSG sits on the odd (low) byte lane of $C00010-$C00017. gpgx
      // takes the write one 68000 cycle before the access (TP-209.2: every
      // PSG record of fm-psg-0..3-once 7 master cycles early, the YM records
      // of the same lists, same instruction forms, exact).
      if (word || (offset & 1U) != 0)
        psg_write(static_cast<std::uint8_t>(value),
                  cpu_.getClock() * kDivider68k - kDivider68k);
      break;
    }
    if (offset < 4)
      vdp_.write_data(v);
    else if (offset < 8) {
      const bool was_on = (vdp_.reg(1) & 0x40U) != 0;
      vdp_.write_control(v);
      update_irq(); // a register write can enable a pending interrupt
      if (line_drawn_ && !was_on && (vdp_.reg(1) & 0x40U) != 0 &&
          !display_on_cycle_)
        display_on_cycle_ = line_cycle();
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
std::uint8_t System::ym_status(std::uint64_t now) {
  return static_cast<std::uint8_t>((now < ym_busy_until_ ? 0x80U : 0U) |
                                   ym_.status(now));
}

void System::log_audio(std::uint16_t addr, std::uint8_t data, std::uint8_t chip,
                       std::uint64_t now) {
  if (audio_events_on_) {
    AudioEvent event{};
    event.t_global = now;
    event.t_frame = static_cast<std::uint32_t>(frame_);
    event.t_cycles = static_cast<std::uint32_t>(now - log_origin_);
    event.source = chip; // 0 FM, 1 PSG
    event.type = static_cast<std::uint8_t>(AudioEventType::raw_write);
    event.channel = kAudioNoChannel;
    event.schema = kAudioEventSchema;
    event.reg = addr;
    event.data = data;
    audio_events_->push(event);
  }
  if (audio_log_.size() >= kAudioWriteCapacity) {
    ++audio_dropped_;
    return;
  }
  audio_log_.push_back(
      {static_cast<std::uint32_t>(now - log_origin_), addr, data, chip});
}

void System::psg_write(std::uint8_t value, std::uint64_t now) {
  psg_.write(value, now);
  log_audio(0, value, 1, now);
}

void System::ym_write(std::uint32_t address, std::uint8_t value,
                      std::uint64_t now, bool word) {
  // A word write puts its high byte on the even address: an address port
  // (gpgx fm_psg_sweep variant 3); the low byte is lost.
  const unsigned port = word ? (address & 2U) : (address & 3U);
  ym_.write(port, value, now);
  if ((port & 1U) == 0) {
    // One latch: the part of the following data writes is the one of the last
    // address write, whatever port the data goes to (gpgx fm_psg_z80).
    ym_addr_ = static_cast<std::uint16_t>(((port >> 1) << 8) | value);
  } else {
    log_audio(ym_addr_, value, 0, now);
  }
  // The busy time was measured on data writes by byte and on every word write
  // (ym-busy-N probes, SoR2 corpus).
  if ((address & 1U) != 0 || word) {
    const std::uint64_t frame_start =
        frame_ * cycles_per_frame(config_.standard);
    const std::uint64_t from = now + kYmBusy - frame_start + kYmClock - kYmEdge;
    ym_busy_until_ = frame_start + (from + kYmClock - 1) / kYmClock * kYmClock -
                     (kYmClock - kYmEdge);
  }
}

void System::z80_bank_write(std::uint8_t value) noexcept {
  z80_bank_ = static_cast<std::uint16_t>(
      ((z80_bank_ >> 1) | ((value & 1U) << 8)) & 0x1FFU);
}

// The 68000 is held for kZ80BusSteal master cycles. The 68000 clock counts in
// 68000 cycles, so each access takes the 68000 cycles whose boundaries fall in
// [z80_master_, z80_master_ + kZ80BusSteal): 72/7 on average and no state.
void System::z80_take_68k_bus() noexcept {
  const std::uint64_t from = (z80_master_ + kDivider68k - 1) / kDivider68k;
  const std::uint64_t to =
      (z80_master_ + kZ80BusSteal + kDivider68k - 1) / kDivider68k;
  cpu_.stall(static_cast<int>(to - from));
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
    z80_take_68k_bus();
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
    if ((address & 1U) != 0)
      ym_busy_until_ += kZ80YmBusyExtra;
  } else if (address < 0x8000) {
    if ((address & 0xFF00U) == 0x6000U)
      z80_bank_write(value);
    else if (address >= 0x7F00U) {
      // The VDP window of the Z80 (68000 bus): 3 wait states for each access,
      // counted before the write is stamped (gpgx fm_psg_z80).
      wait = kZ80VdpWait;
      z80_take_68k_bus();
      if ((address & 0xFFF9U) == 0x7F11U)
        psg_write(value,
                  z80_master_ + std::uint64_t{kZ80VdpWait} * kDividerZ80);
    }
  } else {
    const std::uint32_t a =
        (std::uint32_t{z80_bank_} << 15) | (address & 0x7FFFU);
    const Target t = classify(a);
    // A write through the window waits like a read (z80_steal 0-w and 0: the
    // same 68000 count on gpgx, so the same Z80 pass and the same wait).
    wait = kZ80RomWait;
    z80_take_68k_bus();
    if (t == Target::work_ram || t == Target::rom)
      bus_.write8(a, value);
  }
}

std::vector<std::uint8_t> System::save_state() const {
  std::vector<std::uint8_t> out;
  out.reserve(256 * 1024);
  StateWriter w(out);
  w.u32(kStateMagic);
  w.u32(kStateVersion);
  w.u64(rom_hash_);
  w.u8(static_cast<std::uint8_t>(config_.region));
  w.u8(static_cast<std::uint8_t>(config_.standard));
  w.u8(static_cast<std::uint8_t>(config_.pads[0]));
  w.u8(static_cast<std::uint8_t>(config_.pads[1]));
  w.boolean(config_.tmss);
  w.u8(static_cast<std::uint8_t>(config_.mapper));

  // Machine: scheduling, buses and I/O.
  w.u64(frame_);
  w.u32(current_line_);
  w.u32(frame_start_line_);
  w.u64(line_start_master_);
  w.i64(refresh_due_);
  w.boolean(z80_bus_requested_);
  w.boolean(z80_reset_released_);
  w.u64(z80_master_);
  w.u16(z80_bank_);
  w.u64(z80_int_end_);
  w.bytes(z80_ram_);
  w.array(io_data_);
  w.array(io_ctrl_);
  w.array(pads_);
  for (const auto &pad : pad_port_) {
    w.boolean(pad.th);
    w.u8(pad.phase);
    w.u64(pad.edge_at);
    w.u64(pad.pull_up_at);
  }
  w.u16(ym_addr_);
  w.u64(ym_busy_until_);

  // Components.
  cpu_.save(w);
  bus_.save(w);
  sram_.save(w);
  tmss_.save(w);
  vdp_.save(w);
  renderer_.save(w);
  z80_.save(w);
  ym_.save(w);
  psg_.save(w);
  mixer_.save(w);
  w.u32(kStateEnd);
  return out;
}

bool System::load_state(std::span<const std::uint8_t> state) {
  // All or nothing: a state that fails half way (it is only known to be whole
  // once everything is read) must not leave a half loaded machine.
  const auto backup = save_state();
  if (read_state(state))
    return true;
  const bool restored = read_state(backup);
  (void)restored; // the backup is a state of this machine; it always loads
  return false;
}

bool System::read_state(std::span<const std::uint8_t> state) {
  StateReader r(state);
  if (r.u32() != kStateMagic || r.u32() != kStateVersion ||
      r.u64() != rom_hash_)
    return false;
  const auto region = r.u8(), standard = r.u8(), pad0 = r.u8(), pad1 = r.u8();
  const auto tmss = r.boolean();
  const auto mapper = r.u8();
  if (!r.ok() || region != static_cast<std::uint8_t>(config_.region) ||
      standard != static_cast<std::uint8_t>(config_.standard) ||
      pad0 != static_cast<std::uint8_t>(config_.pads[0]) ||
      pad1 != static_cast<std::uint8_t>(config_.pads[1]) ||
      tmss != config_.tmss ||
      mapper != static_cast<std::uint8_t>(config_.mapper))
    return false;

  frame_ = r.u64();
  current_line_ = r.u32();
  frame_start_line_ = r.u32();
  line_start_master_ = r.u64();
  refresh_due_ = r.i64();
  z80_bus_requested_ = r.boolean();
  z80_reset_released_ = r.boolean();
  z80_master_ = r.u64();
  z80_bank_ = r.u16();
  z80_int_end_ = r.u64();
  r.bytes(z80_ram_);
  r.array(io_data_);
  r.array(io_ctrl_);
  r.array(pads_);
  for (auto &pad : pad_port_) {
    pad.th = r.boolean();
    pad.phase = r.u8();
    pad.edge_at = r.u64();
    pad.pull_up_at = r.u64();
  }
  ym_addr_ = r.u16();
  ym_busy_until_ = r.u64();
  if (current_line_ >= lines_per_frame(config_.standard) ||
      frame_start_line_ >= lines_per_frame(config_.standard) ||
      z80_bank_ > 0x1FF)
    return false;

  cpu_.load(r);
  bus_.load(r);
  sram_.load(r);
  tmss_.load(r);
  vdp_.load(r);
  renderer_.load(r);
  z80_.load(r);
  ym_.load(r);
  psg_.load(r);
  mixer_.load(r);
  if (r.u32() != kStateEnd || !r.finished())
    return false;
  // Per frame outputs are rebuilt by the next frame.
  audio_log_.clear();
  audio_dropped_ = 0;
  update_irq();
  return true;
}

} // namespace tessera::md
