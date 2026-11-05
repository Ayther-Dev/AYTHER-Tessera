// Minimal Mega Drive machine (spec 008, TP-101.9): 68000 + bus + VDP + the
// devices a ROM needs to boot (I/O ports, Z80 bus request, TMSS). The Z80, the
// sound chips and rendering are not here yet; their ports accept writes and
// answer reads so that a ROM can run.
#pragma once
#include "md/bus.hpp"
#include "md/m68k.hpp"
#include "md/mixer.hpp"
#include "md/machine.hpp"
#include "md/tmss.hpp"
#include "md/vdp.hpp"
#include "md/vdp_render.hpp"
#include "md/psg.hpp"
#include "md/ym.hpp"
#include "md/z80.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

// Mega Drive native pad bits (1 = pressed).
inline constexpr std::uint16_t kPadUp = 0x01;
inline constexpr std::uint16_t kPadDown = 0x02;
inline constexpr std::uint16_t kPadLeft = 0x04;
inline constexpr std::uint16_t kPadRight = 0x08;
inline constexpr std::uint16_t kPadB = 0x10;
inline constexpr std::uint16_t kPadC = 0x20;
inline constexpr std::uint16_t kPadA = 0x40;
inline constexpr std::uint16_t kPadStart = 0x80;
// Six-button pad only.
inline constexpr std::uint16_t kPadX = 0x100;
inline constexpr std::uint16_t kPadY = 0x200;
inline constexpr std::uint16_t kPadZ = 0x400;
inline constexpr std::uint16_t kPadMode = 0x800;

/// One write to a sound chip, as the observation region AUDIO_WRITES lists it
/// (TP-107.2). `addr` is (part << 8) | register for the FM and 0 for the PSG.
struct AudioWrite {
  std::uint32_t cycle;
  std::uint16_t addr;
  std::uint8_t data;
  std::uint8_t chip; ///< 0 FM, 1 PSG
};
/// Capacity of the write log of a frame.
inline constexpr std::size_t kAudioWriteCapacity = 8192;

class System final : private BusDevices {
public:
  System(std::vector<std::uint8_t> rom, const MachineConfig &config);

  /// Hard reset: clears RAM and the devices, then resets the CPU.
  void reset();
  void run_frame();

  std::uint64_t frame() const noexcept { return frame_; }
  std::span<const std::uint8_t> z80_ram() const noexcept { return z80_ram_; }
  /// Control write on the Z80_RAM region (TP-103.4): the bytes go straight to
  /// the 8 KiB, the Z80 and the bus are untouched. False if out of range.
  bool poke_z80_ram(std::size_t offset,
                    std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.empty() || offset >= z80_ram_.size() ||
        bytes.size() > z80_ram_.size() - offset)
      return false;
    z80_sync_now(); // the Z80 sees the new bytes from this point on,
    if (z80_running())
      z80_.finish_instruction(); // between two instructions (as for a grant)
    std::copy(bytes.begin(), bytes.end(), z80_ram_.begin() + offset);
    return true;
  }
  std::span<const std::uint8_t> work_ram() const noexcept {
    return bus_.work_ram();
  }
  const Vdp &vdp() const noexcept { return vdp_; }
  /// Frame drawn by the last run_frame (RGBA8, `renderer().size()`).
  const Renderer &renderer() const noexcept { return renderer_; }
  const Sram &sram() const noexcept { return sram_; }
  const M68k &cpu() const noexcept { return cpu_; }

  void set_pad(unsigned port, std::uint16_t pressed) noexcept;

  /// Savestate (TP-110.1, docs/savestate-v1.md): the whole machine at a frame
  /// boundary as a little endian byte stream. The ROM is the content, so the
  /// stream carries only its hash.
  std::vector<std::uint8_t> save_state() const;
  /// Restores a stream written by `save_state` for the same ROM and
  /// configuration. On false (wrong magic, version, ROM, configuration, size or
  /// contents) the machine is left in an unspecified state and must be
  /// discarded or reset; a caller that needs to keep its machine restores into
  /// a new one.
  bool load_state(std::span<const std::uint8_t> state);

  /// Value of the I/O version register ($A10001).
  std::uint8_t io_version() const noexcept;
  /// The 68000 owns the Z80 bus: requested, and the Z80 out of reset.
  bool z80_bus_granted() const noexcept {
    return z80_bus_requested_ && z80_reset_released_;
  }
  const Z80 &z80() const noexcept { return z80_; }
  const Ym2612 &ym() const noexcept { return ym_; }
  const Psg &psg() const noexcept { return psg_; }
  /// Audio of the last frame run: interleaved S16 stereo at 48 kHz (TP-107.4).
  std::span<const std::int16_t> audio() const noexcept { return mixer_.output(); }
  /// `AUDIO_MUTE` (RF-3.7): 18 bits, FM 0-5, PSG 6-9 (tones 0-2, noise), PCM
  /// 10-17 (none on this console). It acts on the mix only: not in the state,
  /// not in the savestate, and it survives reset and restore.
  static constexpr std::uint32_t kAudioMuteBits = 0x3FFFFU;
  void set_audio_mute(std::uint32_t mask) noexcept {
    audio_mute_ = mask & kAudioMuteBits;
    ym_.set_mute(static_cast<std::uint8_t>(audio_mute_ & 0x3FU));
    psg_.set_mute(static_cast<std::uint8_t>(audio_mute_ >> 6 & 0xFU));
  }
  std::uint32_t audio_mute() const noexcept { return audio_mute_; }
  /// Sound chip writes of the last frame run (up to kAudioWriteCapacity) and
  /// the ones that did not fit.
  std::span<const AudioWrite> audio_writes() const noexcept {
    return audio_log_;
  }
  std::uint32_t audio_writes_dropped() const noexcept {
    return audio_dropped_;
  }
  /// Nine bit bank register: A23-A15 of the Z80 window at $8000.
  std::uint16_t z80_bank() const noexcept { return z80_bank_; }

  // Direct 68000 bus access, for tests and tools.
  std::uint8_t read8(std::uint32_t a) { return bus_.read8(a); }
  std::uint16_t read16(std::uint32_t a) { return bus_.read16(a); }
  void write8(std::uint32_t a, std::uint8_t v) { bus_.write8(a, v); }
  void write16(std::uint32_t a, std::uint16_t v) { bus_.write16(a, v); }

private:
  void run_cpu_until(moira::i64 target);
  bool z80_running() const noexcept {
    return z80_reset_released_ && !z80_bus_requested_;
  }
  /// Runs the Z80 up to `master` (it only advances while it owns its bus).
  void z80_sync(std::uint64_t master);
  void z80_sync_now() { z80_sync(cpu_.getClock() * kDivider68k); }
  std::uint8_t z80_read(std::uint16_t address, unsigned &wait);
  void z80_write(std::uint16_t address, std::uint8_t value, unsigned &wait);
  void z80_bank_write(std::uint8_t value) noexcept;
  std::uint16_t open_bus(std::uint32_t offset, bool word) const noexcept;
  moira::i64 refresh_due_ = 0;
  void z80_wait();
  std::uint16_t read(Target target, std::uint32_t offset, bool word) override;
  void write(Target target, std::uint32_t offset, std::uint16_t value,
             bool word) override;
  std::uint8_t io_read(std::uint32_t offset) noexcept;
  void io_write(std::uint32_t offset, std::uint8_t value) noexcept;
  /// Six-button pad: what the pad has seen of TH (TP-108.2). The pad takes TH
  /// from a write, not from the pin; `phase` counts the rising edges of TH
  /// since the pad was last idle.
  struct PadPort {
    bool th = true;
    std::uint8_t phase = 0;
    std::uint64_t edge_at = 0; ///< master cycle of the last change of TH
    std::uint64_t pull_up_at = 0; ///< pending pull-up of TH (0 = none)
  };
  void pad_notify(unsigned port, bool control_write, std::uint64_t now) noexcept;
  void pad_settle(unsigned port, std::uint64_t now) noexcept;
  std::array<PadPort, 2> pad_port_{};
  bool th2() const noexcept;
  std::uint32_t line_cycle() const noexcept;
  void update_irq() noexcept;

  MachineConfig config_;
  std::uint64_t rom_hash_ = 0; ///< FNV-1a of the ROM image, for the savestate
  Sram sram_; ///< declared before the bus: built from the ROM header first
  Bus bus_;
  M68k cpu_;
  Vdp vdp_;
  Tmss tmss_;
  class Z80Port final : public Z80Bus {
  public:
    explicit Z80Port(System &system) : system_(system) {}
    std::uint8_t read(std::uint16_t address, unsigned &wait) override {
      return system_.z80_read(address, wait);
    }
    void write(std::uint16_t address, std::uint8_t value,
               unsigned &wait) override {
      system_.z80_write(address, value, wait);
    }

  private:
    System &system_;
  };
  Z80Port z80_port_{*this};
  Z80 z80_{z80_port_};
  std::uint64_t z80_master_ = 0; ///< master cycle the Z80 has run up to
  std::uint16_t z80_bank_ = 0;
  Ym2612 ym_;
  Psg psg_;
  Mixer mixer_;
  std::uint32_t audio_mute_ = 0;
  std::vector<AudioWrite> audio_log_;
  std::uint32_t audio_dropped_ = 0;
  std::uint64_t log_origin_ = 0; ///< master cycle the log counts from
  std::uint16_t ym_addr_ = 0; ///< latch: (part << 8) | register of the last address write
  void log_audio(std::uint16_t addr, std::uint8_t data, std::uint8_t chip,
                 std::uint64_t now);
  void psg_write(std::uint8_t value, std::uint64_t now);
  std::uint64_t ym_busy_until_ = 0; ///< master cycle the YM busy flag drops
  std::uint8_t ym_status(std::uint64_t now);
  void ym_write(std::uint32_t address, std::uint8_t value, std::uint64_t now,
                bool word);
  std::uint64_t z80_int_end_ = 0; ///< master cycle the INT pulse ends
  Renderer renderer_;
  std::uint64_t frame_ = 0;
  std::uint32_t current_line_ = 0;
  std::uint32_t frame_start_line_ = 192; // gpgx starts in Mode 4
  std::uint64_t line_start_master_ = 0;

  bool z80_bus_requested_ = false;
  bool z80_reset_released_ = false;
  std::array<std::uint8_t, 0x2000> z80_ram_{};
  // I/O: data and control registers of port 1, port 2 and the expansion port.
  std::array<std::uint8_t, 3> io_data_{0x00, 0x00, 0x00};
  std::array<std::uint8_t, 3> io_ctrl_{0x00, 0x00, 0x00};
  std::array<std::uint16_t, 2> pads_{0x00, 0x00};
};

} // namespace tessera::md
