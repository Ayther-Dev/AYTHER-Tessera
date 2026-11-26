#include "md/m68k.hpp"

namespace tessera::md {

M68k::M68k(Bus &bus) : bus_(bus) { setModel(moira::Model::M68000); }

void M68k::willInterrupt(moira::u8 level) {
  if (interrupt_hook_)
    sync(interrupt_hook_(level, clock));
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

void M68k::save(StateWriter &w) const {
  w.i64(clock);
  w.u32(reg.pc);
  w.u32(reg.pc0);
  const auto &sr = reg.sr;
  for (const bool flag :
       {sr.t1, sr.t0, sr.s, sr.m, sr.x, sr.n, sr.z, sr.v, sr.c})
    w.boolean(flag);
  w.u8(sr.ipl);
  for (const auto v : reg.r)
    w.u32(v);
  w.u32(reg.usp);
  w.u32(reg.isp);
  w.u32(reg.msp);
  w.u8(reg.ipl);
  w.u32(reg.vbr);
  w.u32(reg.sfc);
  w.u32(reg.dfc);
  w.u32(reg.cacr);
  w.u32(reg.caar);
  w.u16(queue.irc);
  w.u16(queue.ird);
  w.u8(static_cast<std::uint8_t>(irqMode));
  w.u8(ipl);
  w.u8(fcl);
  w.u8(fcSource);
  w.i32(exception);
  w.i32(loopModeDelay);
  w.u16(readBuffer);
  w.u16(writeBuffer);
  w.i32(flags);
}

void M68k::load(StateReader &r) {
  clock = r.i64();
  reg.pc = r.u32();
  reg.pc0 = r.u32();
  auto &sr = reg.sr;
  for (bool *flag :
       {&sr.t1, &sr.t0, &sr.s, &sr.m, &sr.x, &sr.n, &sr.z, &sr.v, &sr.c})
    *flag = r.boolean();
  sr.ipl = r.u8();
  for (auto &v : reg.r)
    v = r.u32();
  reg.usp = r.u32();
  reg.isp = r.u32();
  reg.msp = r.u32();
  reg.ipl = r.u8();
  reg.vbr = r.u32();
  reg.sfc = r.u32();
  reg.dfc = r.u32();
  reg.cacr = r.u32();
  reg.caar = r.u32();
  queue.irc = r.u16();
  queue.ird = r.u16();
  const auto mode = r.u8();
  if (mode > static_cast<std::uint8_t>(moira::IrqMode::UNINITIALIZED))
    r.fail();
  irqMode = static_cast<moira::IrqMode>(mode);
  ipl = r.u8();
  fcl = r.u8();
  fcSource = r.u8();
  exception = r.i32();
  loopModeDelay = r.i32();
  readBuffer = r.u16();
  writeBuffer = r.u16();
  flags = r.i32();
}

} // namespace tessera::md
