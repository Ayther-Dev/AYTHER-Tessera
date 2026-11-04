// 68000 adapter of the native Mega Drive core (spec 008, TP-101.8).
// Moira (MIT, pinned in cmake/dependencies.lock.json) executes the
// instructions; every access goes through the Mega Drive `Bus`. Reset,
// exceptions and the interrupt priority level are Moira's own (`reset()`,
// `setIPL()`); this class only supplies the bus and the clock.
#pragma once
#include "md/bus.hpp"

#include "Moira.h"

namespace tessera::md {

class M68k final : public moira::Moira {
public:
  explicit M68k(Bus &bus);

protected:
  moira::u8 read8(moira::u32 addr) const override;
  moira::u16 read16(moira::u32 addr) const override;
  moira::u32 read32(moira::u32 addr) const override;
  void write8(moira::u32 addr, moira::u8 val) const override;
  void write16(moira::u32 addr, moira::u16 val) const override;
  void write32(moira::u32 addr, moira::u32 val) const override;

private:
  Bus &bus_;
};

} // namespace tessera::md
