/* Compiles floooh/chips z80.h (zlib) as C. See z80_chips.h. */
#include "md/z80_chips.h"

#define CHIPS_IMPL
#include <z80.h>

_Static_assert(sizeof(z80_t) <= TESSERA_Z80_STATE_BYTES,
               "TESSERA_Z80_STATE_BYTES is too small for z80_t");
_Static_assert(_Alignof(z80_t) <= 8, "z80_t alignment");

#define CPU(p) ((z80_t *)(p))
#define CCPU(p) ((const z80_t *)(p))

uint64_t tessera_z80_init(tessera_z80 *cpu) { return z80_init(CPU(cpu)); }
uint64_t tessera_z80_reset(tessera_z80 *cpu) { return z80_reset(CPU(cpu)); }
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
