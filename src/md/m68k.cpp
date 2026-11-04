#include "md/m68k.hpp"

namespace tessera::md {

M68k::M68k(Bus &bus) : bus_(bus) { setModel(moira::Model::M68000); }

void M68k::willInterrupt(moira::u8 level) {
  if (interrupt_hook_)
    interrupt_hook_(level);
}

moira::u8 M68k::read8(moira::u32 addr) const { return bus_.read8(addr); }
moira::u16 M68k::read16(moira::u32 addr) const { return bus_.read16(addr); }
moira::u32 M68k::read32(moira::u32 addr) const { return bus_.read32(addr); }
void M68k::write8(moira::u32 addr, moira::u8 val) const {
  bus_.write8(addr, val);
}
void M68k::write16(moira::u32 addr, moira::u16 val) const {
  bus_.write16(addr, val);
}
void M68k::write32(moira::u32 addr, moira::u32 val) const {
  bus_.write32(addr, val);
}

} // namespace tessera::md
