// 68000 adapter of the native Mega Drive core (spec 008, TP-101.8).
// Moira (MIT, pinned in cmake/dependencies.lock.json) executes the
// instructions; every access goes through the Mega Drive `Bus`. Reset,
// exceptions and the interrupt priority level are Moira's own (`reset()`,
// `setIPL()`); this class only supplies the bus and the clock.
#pragma once
#include "md/bus.hpp"
#include "md/state_io.hpp"

#include "Moira.h"

#include <functional>

namespace tessera::md {

class M68k final : public moira::Moira {
public:
  explicit M68k(Bus &bus);

  /// Called when the CPU is about to take an interrupt of the given level.
  void set_interrupt_hook(std::function<void(std::uint8_t)> hook) {
    interrupt_hook_ = std::move(hook);
  }

  /// Holds the CPU for `cycles` 68000 cycles (a DMA owns the bus).
  void stall(int cycles) { sync(cycles); }

  /// Savestate: clock, registers, prefetch queue and interrupt lines.
  void save(StateWriter &w) const;
  void load(StateReader &r);

protected:
  void willInterrupt(moira::u8 level) override;
  moira::u8 read8(moira::u32 addr) const override;
  moira::u16 read16(moira::u32 addr) const override;
  moira::u32 read32(moira::u32 addr) const override;
  void write8(moira::u32 addr, moira::u8 val) const override;
  void write16(moira::u32 addr, moira::u16 val) const override;
  void write32(moira::u32 addr, moira::u32 val) const override;

private:
  Bus &bus_;
  std::function<void(std::uint8_t)> interrupt_hook_;
};

} // namespace tessera::md
