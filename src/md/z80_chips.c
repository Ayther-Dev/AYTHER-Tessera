/* Compiles floooh/chips z80.h (zlib) as C. See z80_chips.h. */
#include "md/z80_chips.h"

#define CHIPS_IMPL
#include <z80.h>

_Static_assert(sizeof(z80_t) <= TESSERA_Z80_STATE_BYTES,
               "TESSERA_Z80_STATE_BYTES is too small for z80_t");
_Static_assert(_Alignof(z80_t) <= 8, "z80_t alignment");

#define CPU(p) ((z80_t *)(p))
#define CCPU(p) ((const z80_t *)(p))

/* z80.h resets the registers to $FFFF. The Mega Drive Z80 comes out of reset
 * with every register at 0 (measured against gpgx, z80_bank). */
static void clear_registers(z80_t *c) {
  c->af = c->bc = c->de = c->hl = 0;
  c->wz = c->sp = c->ix = c->iy = 0;
  c->af2 = c->bc2 = c->de2 = c->hl2 = 0;
}

uint64_t tessera_z80_init(tessera_z80 *cpu) {
  const uint64_t pins = z80_init(CPU(cpu));
  clear_registers(CPU(cpu));
  return pins;
}
uint64_t tessera_z80_reset(tessera_z80 *cpu) {
  const uint64_t pins = z80_reset(CPU(cpu));
  clear_registers(CPU(cpu));
  return pins;
}
/* The RESET line only restarts the program counter, I, R, the interrupt mode
 * and the flip-flops; the other registers keep their value (gpgx, z80_bank). */
uint64_t tessera_z80_soft_reset(tessera_z80 *cpu) {
  const z80_t saved = *CPU(cpu);
  const uint64_t pins = z80_reset(CPU(cpu));
  z80_t *c = CPU(cpu);
  c->af = saved.af; c->bc = saved.bc; c->de = saved.de; c->hl = saved.hl;
  c->ix = saved.ix; c->iy = saved.iy; c->sp = saved.sp; c->wz = saved.wz;
  c->af2 = saved.af2; c->bc2 = saved.bc2; c->de2 = saved.de2; c->hl2 = saved.hl2;
  return pins;
}
uint64_t tessera_z80_tick(tessera_z80 *cpu, uint64_t pins) {
  return z80_tick(CPU(cpu), pins);
}
int tessera_z80_opdone(tessera_z80 *cpu) { return z80_opdone(CPU(cpu)); }

uint16_t tessera_z80_pc(const tessera_z80 *cpu) { return CCPU(cpu)->pc; }
uint16_t tessera_z80_sp(const tessera_z80 *cpu) { return CCPU(cpu)->sp; }
uint16_t tessera_z80_af(const tessera_z80 *cpu) { return CCPU(cpu)->af; }
uint16_t tessera_z80_bc(const tessera_z80 *cpu) { return CCPU(cpu)->bc; }
uint16_t tessera_z80_de(const tessera_z80 *cpu) { return CCPU(cpu)->de; }
uint16_t tessera_z80_hl(const tessera_z80 *cpu) { return CCPU(cpu)->hl; }
uint16_t tessera_z80_ix(const tessera_z80 *cpu) { return CCPU(cpu)->ix; }
uint16_t tessera_z80_iy(const tessera_z80 *cpu) { return CCPU(cpu)->iy; }
uint8_t tessera_z80_im(const tessera_z80 *cpu) { return CCPU(cpu)->im; }
int tessera_z80_iff1(const tessera_z80 *cpu) { return CCPU(cpu)->iff1; }
int tessera_z80_halted(const tessera_z80 *cpu) {
  return (CCPU(cpu)->pins & Z80_HALT) != 0;
}
