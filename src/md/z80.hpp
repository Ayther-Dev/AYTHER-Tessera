// Z80 adapter of the native Mega Drive core (spec 008, TP-103.1).
// floooh/chips `z80.h` (zlib, compiled as C in z80_chips.c) executes the
// instructions one T-state at a time; this class feeds it the memory bus of the
// Z80 side and the INT and WAIT pins. The Mega Drive wiring (RAM, YM, PSG, bank
// window) is `Z80Bus` in system.cpp.
#pragma once
#include "md/z80_chips.h"

#include <cstdint>

namespace tessera::md {

/// What the Z80 sees on its address space. `read` and `write` are called once
/// per memory cycle; each returns the extra wait T-states of that access.
class Z80Bus {
public:
  virtual ~Z80Bus() = default;
  virtual std::uint8_t read(std::uint16_t address, unsigned &wait) = 0;
  virtual void write(std::uint16_t address, std::uint8_t value,
                     unsigned &wait) = 0;
};

class Z80 {
public:
  explicit Z80(Z80Bus &bus);

  /// Power-on state: PC = 0 and every register 0; pending waits are dropped.
  void reset();
  /// The RESET line: PC = 0, the other registers keep their value.
  void soft_reset();
  /// Executes one T-state.
  void tick();
  /// Ticks until the instruction in flight is complete (at most 32 T-states).
  void finish_instruction();
  /// Level of the INT pin (kept until the 68000 side lowers it).
  void set_int(bool level) noexcept { int_ = level; }

  std::uint64_t cycles() const noexcept { return cycles_; }
  std::uint16_t pc() const noexcept { return tessera_z80_pc(&cpu_); }
  std::uint16_t sp() const noexcept { return tessera_z80_sp(&cpu_); }
  std::uint16_t af() const noexcept { return tessera_z80_af(&cpu_); }
  std::uint16_t bc() const noexcept { return tessera_z80_bc(&cpu_); }
  std::uint16_t de() const noexcept { return tessera_z80_de(&cpu_); }
  std::uint16_t hl() const noexcept { return tessera_z80_hl(&cpu_); }
  std::uint16_t ix() const noexcept { return tessera_z80_ix(&cpu_); }
  std::uint16_t iy() const noexcept { return tessera_z80_iy(&cpu_); }
  std::uint8_t im() const noexcept { return tessera_z80_im(&cpu_); }
  bool iff1() const noexcept { return tessera_z80_iff1(&cpu_) != 0; }
  bool halted() const noexcept { return tessera_z80_halted(&cpu_) != 0; }

private:
  Z80Bus &bus_;
  tessera_z80 cpu_{};
  std::uint64_t pins_ = 0;
  std::uint64_t cycles_ = 0;
  unsigned wait_ = 0;
  bool int_ = false;
};

} // namespace tessera::md
