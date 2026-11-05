/* C interface to floooh/chips z80.h (zlib), spec 008, TP-103.1. The vendored
 * header is compiled as C in z80_chips.c; the C++ side never includes it. The
 * Z80 state is an opaque block of TESSERA_Z80_STATE_BYTES (a static assert in
 * z80_chips.c keeps it large enough for `z80_t`). */
#ifndef TESSERA_MD_Z80_CHIPS_H
#define TESSERA_MD_Z80_CHIPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TESSERA_Z80_STATE_BYTES 160

typedef struct tessera_z80 {
  _Alignas(8) unsigned char bytes[TESSERA_Z80_STATE_BYTES];
} tessera_z80;

/* Pin masks, as in z80.h. */
#define TZ80_MREQ (1ULL << 25)
#define TZ80_IORQ (1ULL << 26)
#define TZ80_RD (1ULL << 27)
#define TZ80_WR (1ULL << 28)
#define TZ80_INT (1ULL << 30)
#define TZ80_WAIT (1ULL << 33)

/* Each returns the pins to feed to the next tick. */
uint64_t tessera_z80_init(tessera_z80 *cpu);
uint64_t tessera_z80_reset(tessera_z80 *cpu);
uint64_t tessera_z80_tick(tessera_z80 *cpu, uint64_t pins);
int tessera_z80_opdone(tessera_z80 *cpu);

uint16_t tessera_z80_pc(const tessera_z80 *cpu);
uint16_t tessera_z80_sp(const tessera_z80 *cpu);
uint16_t tessera_z80_af(const tessera_z80 *cpu);
uint16_t tessera_z80_bc(const tessera_z80 *cpu);
uint16_t tessera_z80_de(const tessera_z80 *cpu);
uint16_t tessera_z80_hl(const tessera_z80 *cpu);
uint16_t tessera_z80_ix(const tessera_z80 *cpu);
uint16_t tessera_z80_iy(const tessera_z80 *cpu);
uint8_t tessera_z80_im(const tessera_z80 *cpu);
int tessera_z80_iff1(const tessera_z80 *cpu);
int tessera_z80_halted(const tessera_z80 *cpu);

#ifdef __cplusplus
}
#endif
#endif
