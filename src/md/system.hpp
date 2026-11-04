// Minimal Mega Drive machine (spec 008, TP-101.9): 68000 + bus + VDP + the
// devices a ROM needs to boot (I/O ports, Z80 bus request, TMSS). The Z80, the
// sound chips and rendering are not here yet; their ports accept writes and
// answer reads so that a ROM can run.
#pragma once
#include "md/bus.hpp"
#include "md/m68k.hpp"
#include "md/machine.hpp"
#include "md/tmss.hpp"
#include "md/vdp.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

// Mega Drive native pad bits (1 = pressed).
inline constexpr std::uint8_t kPadUp = 0x01;
inline constexpr std::uint8_t kPadDown = 0x02;
inline constexpr std::uint8_t kPadLeft = 0x04;
inline constexpr std::uint8_t kPadRight = 0x08;
inline constexpr std::uint8_t kPadB = 0x10;
inline constexpr std::uint8_t kPadC = 0x20;
inline constexpr std::uint8_t kPadA = 0x40;
inline constexpr std::uint8_t kPadStart = 0x80;

class System final : private BusDevices {
public:
  System(std::vector<std::uint8_t> rom, const MachineConfig &config);

  /// Hard reset: clears RAM and the devices, then resets the CPU.
  void reset();
  void run_frame();

  std::uint64_t frame() const noexcept { return frame_; }
  std::span<const std::uint8_t> work_ram() const noexcept {
    return bus_.work_ram();
  }
  const Vdp &vdp() const noexcept { return vdp_; }
  const M68k &cpu() const noexcept { return cpu_; }

  void set_pad(unsigned port, std::uint8_t pressed) noexcept;

  /// Value of the I/O version register ($A10001).
  std::uint8_t io_version() const noexcept;
  bool z80_bus_granted() const noexcept { return z80_bus_requested_; }

  // Direct 68000 bus access, for tests and tools.
  std::uint8_t read8(std::uint32_t a) { return bus_.read8(a); }
  std::uint16_t read16(std::uint32_t a) { return bus_.read16(a); }
  void write8(std::uint32_t a, std::uint8_t v) { bus_.write8(a, v); }
  void write16(std::uint32_t a, std::uint16_t v) { bus_.write16(a, v); }

private:
  std::uint16_t read(Target target, std::uint32_t offset, bool word) override;
  void write(Target target, std::uint32_t offset, std::uint16_t value,
             bool word) override;
  std::uint8_t io_read(std::uint32_t offset) const noexcept;
  void io_write(std::uint32_t offset, std::uint8_t value) noexcept;
  void update_irq() noexcept;

  MachineConfig config_;
  Bus bus_;
  M68k cpu_;
  Vdp vdp_;
  Tmss tmss_;
  std::uint64_t frame_ = 0;
  std::uint32_t current_line_ = 0;
  std::uint64_t line_start_master_ = 0;

  bool z80_bus_requested_ = false;
  bool z80_reset_released_ = false;
  std::array<std::uint8_t, 0x2000> z80_ram_{};
  // I/O: data and control registers of port 1, port 2 and the expansion port.
  std::array<std::uint8_t, 3> io_data_{0x00, 0x00, 0x00};
  std::array<std::uint8_t, 3> io_ctrl_{0x00, 0x00, 0x00};
  std::array<std::uint8_t, 2> pads_{0x00, 0x00};
};

} // namespace tessera::md
