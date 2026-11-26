// Synthetic Mega Drive test ROMs for the Z80 side (spec 008, TP-103.2/103.3).
// A 68000 program uploads a Z80 payload through the bus request / reset
// sequence games use; the Z80 payload exercises its address space and leaves
// the results in its RAM and, through the bank window, in the 68000 work RAM.
// The probes carry no commercial content.
#pragma once
#include "support/md_testrom.hpp"

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace tessera::testrom {

// A tiny Z80 assembler: only what the probes need.
class Z80Asm {
public:
  std::size_t here() const { return code.size(); }
  void db(std::initializer_list<std::uint8_t> bytes) {
    code.insert(code.end(), bytes);
  }
  void org(std::size_t at) { code.resize(at, 0); }
  void ld_sp(std::uint16_t n) { db({0x31, lo(n), hi(n)}); }
  void ld_a(std::uint8_t n) { db({0x3E, n}); }
  void ld_hl(std::uint16_t n) { db({0x21, lo(n), hi(n)}); }
  void ld_de(std::uint16_t n) { db({0x11, lo(n), hi(n)}); }
  void ld_bc(std::uint16_t n) { db({0x01, lo(n), hi(n)}); }
  void ld_mem_a(std::uint16_t n) { db({0x32, lo(n), hi(n)}); }
  void ld_a_mem(std::uint16_t n) { db({0x3A, lo(n), hi(n)}); }
  void ld_mem_hl(std::uint16_t n) { db({0x22, lo(n), hi(n)}); }
  void inc_a() { db({0x3C}); }
  void inc_hl() { db({0x23}); }
  void ldir() { db({0xED, 0xB0}); }
  void call(std::uint16_t n) { db({0xCD, lo(n), hi(n)}); }
  void ret() { db({0xC9}); }
  void jr_self() { db({0x18, 0xFE}); }
  void jr_to(std::size_t target) {
    db({0x18, static_cast<std::uint8_t>(static_cast<int>(target) -
                                        static_cast<int>(here() + 2))});
  }
  std::vector<std::uint8_t> code;

private:
  static std::uint8_t lo(std::uint16_t n) {
    return static_cast<std::uint8_t>(n);
  }
  static std::uint8_t hi(std::uint16_t n) {
    return static_cast<std::uint8_t>(n >> 8);
  }
};

// Bank register routine: HL = address written, DE = 9 bit bank (bit 0 first).
inline void z80_setbank_routine(Z80Asm &a) {
  a.db({0x06, 0x09}); // ld b,9
  a.db({0x73});       // l: ld (hl),e
  a.db({0xCB, 0x3A}); //    srl d
  a.db({0xCB, 0x1B}); //    rr e
  a.db({0x10, 0xF9}); //    djnz l
  a.ret();
}

inline constexpr std::uint16_t kZ80SetBank = 0x0300;
inline constexpr std::uint32_t kPayloadAt =
    0x800; // in the ROM, past the 68000 code

// The payload of `z80_bank`: results at $1000-$10FF of the Z80 RAM, a start
// counter at $1FF1 and the end flag $D0 at $1FF0.
inline std::vector<std::uint8_t> z80_bank_payload() {
  Z80Asm a;
  // Registers as the reset left them, stored at $1100.
  a.ld_mem_hl(0x1100);
  a.db({0xED, 0x73, 0x02, 0x11}); // ld ($1102),sp
  a.ld_mem_a(0x1104);
  a.db({0xED, 0x43, 0x05, 0x11}); // ld ($1105),bc
  a.db({0xED, 0x53, 0x07, 0x11}); // ld ($1107),de
  a.db({0xDD, 0x22, 0x09, 0x11}); // ld ($1109),ix
  a.db({0xFD, 0x22, 0x0B, 0x11}); // ld ($110B),iy
  a.ld_sp(0x1FFF);
  a.ld_a_mem(0x1FF1); // start counter (RAM survives a Z80 reset)
  a.inc_a();
  a.ld_mem_a(0x1FF1);
  // T1: the RAM is mirrored at $2000
  a.ld_a(0x5A);
  a.ld_mem_a(0x3800);
  a.ld_a_mem(0x1800);
  a.ld_mem_a(0x1000);
  a.ld_a(0xA5);
  a.ld_mem_a(0x1801);
  a.ld_a_mem(0x3801);
  a.ld_mem_a(0x1001);
  const auto set_bank = [&](std::uint16_t reg, std::uint16_t bank) {
    a.ld_hl(reg);
    a.ld_de(bank);
    a.call(kZ80SetBank);
  };
  // T2: write and read back work RAM through the window
  set_bank(0x6000, 0x1FE);
  a.ld_a(0x11);
  a.ld_mem_a(0x9000);
  a.ld_a(0x22);
  a.ld_mem_a(0x9001);
  a.ld_a(0x33);
  a.ld_mem_a(0xFFFF);
  set_bank(0x6000, 0x1FF);
  a.ld_a(0x44);
  a.ld_mem_a(0x8000);
  a.ld_a(0x55);
  a.ld_mem_a(0x8100);
  set_bank(0x6000, 0x1FE);
  a.ld_a_mem(0x9000);
  a.ld_mem_a(0x1010);
  a.ld_a_mem(0x9001);
  a.ld_mem_a(0x1011);
  a.ld_a_mem(0xFFFF);
  a.ld_mem_a(0x1012);
  set_bank(0x6000, 0x1FF);
  a.ld_a_mem(0x8000);
  a.ld_mem_a(0x1013);
  a.ld_a_mem(0x8100);
  a.ld_mem_a(0x1014);
  // T5: bytes the 68000 left in work RAM at $FF0100
  set_bank(0x6000, 0x1FE);
  for (std::uint16_t i = 0; i < 8; ++i) {
    a.ld_a_mem(static_cast<std::uint16_t>(0x8100 + i));
    a.ld_mem_a(static_cast<std::uint16_t>(0x1020 + i));
  }
  // T3: ROM header through bank 0
  set_bank(0x6000, 0x000);
  a.ld_hl(0x8100);
  a.ld_de(0x1030);
  a.ld_bc(0x0010);
  a.ldir();
  // T4: decoding of the bank register address
  set_bank(0x6001, 0x1FE);
  a.ld_a(0x66);
  a.ld_mem_a(0x9000);
  set_bank(0x60FF, 0x1FF);
  a.ld_a(0x77);
  a.ld_mem_a(0x8002);
  set_bank(0x6000, 0x000);
  set_bank(0x6100, 0x1FE);
  a.ld_a(0x88);
  a.ld_mem_a(0x9003);
  set_bank(0x6000, 0x000);
  set_bank(0x7000, 0x1FE);
  a.ld_a(0x99);
  a.ld_mem_a(0x9004);
  set_bank(0x6000, 0x1FE);
  a.ld_a_mem(0x9000);
  a.ld_mem_a(0x1040);
  a.ld_a_mem(0x9003);
  a.ld_mem_a(0x1042);
  a.ld_a_mem(0x9004);
  a.ld_mem_a(0x1043);
  set_bank(0x6000, 0x1FF);
  a.ld_a_mem(0x8002);
  a.ld_mem_a(0x1041);
  // a tenth bank write shifts the register once more
  set_bank(0x6000, 0x1FE);
  a.ld_hl(0x6000);
  a.ld_a(0x01);
  a.db({0x77}); // ld (hl),a: bank becomes $1FF
  a.ld_a(0xAA);
  a.ld_mem_a(0x9005);
  set_bank(0x6000, 0x1FF);
  a.ld_a_mem(0x8005);
  a.ld_mem_a(0x1044);
  // T7: YM2612 ports
  for (std::uint16_t i = 0; i < 4; ++i) {
    a.ld_a_mem(static_cast<std::uint16_t>(0x4000 + i));
    a.ld_mem_a(static_cast<std::uint16_t>(0x1050 + i));
  }
  a.ld_a_mem(0x5000);
  a.ld_mem_a(0x1054);
  // T9: reads of addresses nothing drives
  a.ld_a_mem(0x6000);
  a.ld_mem_a(0x1060);
  a.ld_a_mem(0x6100);
  a.ld_mem_a(0x1061);
  a.ld_a_mem(0x7000);
  a.ld_mem_a(0x1062);
  a.ld_a_mem(0x7F20);
  a.ld_mem_a(0x1063);
  // PSG, then the end flag
  a.ld_a(0x9F);
  a.ld_mem_a(0x7F11);
  a.ld_a(0xD0);
  a.ld_mem_a(0x1FF0);
  a.jr_self();
  a.org(kZ80SetBank);
  z80_setbank_routine(a);
  return a.code;
}

// Counter payload of `z80_clock`: HL counts in a loop and is stored at $1F00;
// with `bank_reads` every pass also reads a ROM byte through the window.
inline std::vector<std::uint8_t> z80_clock_payload(bool bank_reads) {
  Z80Asm a;
  a.ld_sp(0x1FFF);
  a.ld_hl(0);
  a.ld_mem_hl(0x1F00);
  if (bank_reads) {
    a.ld_hl(0x6000);
    a.ld_de(0x0000);
    a.call(kZ80SetBank);
    a.ld_hl(0);
  }
  const std::size_t loop = a.here();
  a.inc_hl();
  if (bank_reads)
    a.ld_a_mem(0x8000);
  a.ld_mem_hl(0x1F00);
  a.jr_to(loop);
  a.org(kZ80SetBank);
  z80_setbank_routine(a);
  return a.code;
}

namespace detail {
struct M68kProgram {
  std::vector<std::uint16_t> c;
  void w(std::uint16_t v) { c.push_back(v); }
  void l(std::uint32_t v) {
    c.push_back(static_cast<std::uint16_t>(v >> 16));
    c.push_back(static_cast<std::uint16_t>(v));
  }
  void request(std::uint16_t v) { // move.w #v,($A11100)
    w(0x38BC);
    w(v);
  }
  void reset_pin(std::uint16_t v) { // move.w #v,($A11200)
    w(0x3ABC);
    w(v);
  }
  void delay(std::uint16_t n) { // move.w #n,d0 ; dbra d0,*
    w(0x303C);
    w(n);
    w(0x51C8);
    w(0xFFFE);
  }
  void log_status() { // move.w (a4),d3 ; move.w d3,(a2)+
    w(0x3614);
    w(0x34C3);
  }
  // Bounded wait for the grant; logs the poll counter left in d2.
  void wait_grant() {
    w(0x343C); // move.w #1000,d2
    w(1000);
    w(0x0814); // btst #0,(a4)
    w(0x0000);
    w(0x57CA); // dbeq d2,*-6
    w(0xFFFA);
    w(0x34C2); // move.w d2,(a2)+
  }
  void upload(std::uint32_t source, std::uint16_t length) {
    w(0x41F9); // lea source,a0
    l(source);
    w(0x43F9); // lea $A00000,a1
    l(0x00A00000);
    w(0x303C); // move.w #length-1,d0
    w(static_cast<std::uint16_t>(length - 1));
    w(0x12D8); // copy: move.b (a0)+,(a1)+
    w(0x51C8); //       dbra d0,copy
    w(0xFFFC);
  }
  void prologue() {
    w(0x45F9); // lea $FF0000,a2 (log pointer)
    l(0x00FF0000);
    w(0x49F9); // lea $A11100,a4
    l(0x00A11100);
    w(0x4BF9); // lea $A11200,a5
    l(0x00A11200);
  }
};

inline std::vector<std::uint8_t>
z80_rom_image(const char *name, const std::vector<std::uint8_t> &payload,
              const detail::M68kProgram &p) {
  std::vector<std::uint8_t> rom(0x1000, 0);
  put32(rom, 0, 0xFFFE00);
  put32(rom, 4, 0x200);
  std::memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  std::memset(&rom[0x120], ' ', 16);
  std::memcpy(&rom[0x120], name, std::min<std::size_t>(16, std::strlen(name)));
  put32(rom, 0x1A4, 0xFFF);
  std::memset(&rom[0x1F0], ' ', 16);
  rom[0x1F0] = 'U';
  std::memcpy(&rom[kPayloadAt], payload.data(), payload.size());
  std::size_t at = 0x200;
  for (const auto word : p.c) {
    put16(rom, at, word);
    at += 2;
  }
  return rom;
}
} // namespace detail

// `z80_bank` sequences (TP-103.2/103.3): 0 request, release reset, upload,
// reset pulse and release; 1 upload with the Z80 in its power-on reset; 2 reset
// pulse without the bus while it runs; 3 bus request and release while it
// runs; 4 reset pulse while holding the bus.
inline std::vector<std::uint8_t> z80_bank_rom(unsigned sequence) {
  const auto payload = z80_bank_payload();
  const auto length = static_cast<std::uint16_t>(payload.size());
  detail::M68kProgram p;
  p.prologue();
  p.w(0x41F9); // lea $FF0100,a0 (bytes for the Z80 to read)
  p.l(0x00FF0100);
  p.w(0x20FC); // move.l #..,(a0)+
  p.l(0xA1B2C3D4);
  p.w(0x20FC);
  p.l(0xE5F60718);
  switch (sequence) {
  default:
    p.request(0x0100);
    p.reset_pin(0x0100);
    p.wait_grant();
    p.upload(kPayloadAt, length);
    p.reset_pin(0x0000);
    p.reset_pin(0x0100);
    p.request(0x0000);
    p.delay(2000);
    p.log_status();
    break;
  case 1:
    p.request(0x0100);
    p.wait_grant();
    p.log_status();
    p.upload(kPayloadAt, length);
    p.request(0x0000);
    p.reset_pin(0x0100);
    p.delay(2000);
    p.log_status();
    break;
  case 2:
    p.request(0x0100);
    p.reset_pin(0x0100);
    p.wait_grant();
    p.upload(kPayloadAt, length);
    p.reset_pin(0x0000);
    p.reset_pin(0x0100);
    p.request(0x0000);
    p.delay(2000);
    p.reset_pin(0x0000);
    p.delay(10);
    p.log_status();
    p.reset_pin(0x0100);
    p.delay(2000);
    p.log_status();
    break;
  case 3:
    p.request(0x0100);
    p.reset_pin(0x0100);
    p.wait_grant();
    p.upload(kPayloadAt, length);
    p.reset_pin(0x0000);
    p.reset_pin(0x0100);
    p.request(0x0000);
    p.delay(50);
    p.request(0x0100);
    p.wait_grant();
    p.log_status();
    p.delay(500);
    p.request(0x0000);
    p.delay(2000);
    p.log_status();
    break;
  case 4:
    p.request(0x0100);
    p.reset_pin(0x0100);
    p.wait_grant();
    p.upload(kPayloadAt, length);
    p.reset_pin(0x0000);
    p.reset_pin(0x0100);
    p.request(0x0000);
    p.delay(2000);
    p.request(0x0100);
    p.wait_grant();
    p.reset_pin(0x0000);
    p.delay(20);
    p.reset_pin(0x0100);
    p.log_status();
    p.request(0x0000);
    p.delay(2000);
    p.log_status();
    break;
  }
  p.w(0x60FE); // bra.s *
  return detail::z80_rom_image("AYTHER Z80 BANK", payload, p);
}

// `z80_clock`: the Z80 counts; after a growing delay the 68000 requests the
// bus, reads the counter and logs it (16 samples).
inline std::vector<std::uint8_t> z80_clock_rom(bool bank_reads) {
  const auto payload = z80_clock_payload(bank_reads);
  detail::M68kProgram p;
  p.prologue();
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(payload.size()));
  p.reset_pin(0x0000);
  p.reset_pin(0x0100);
  p.request(0x0000);
  for (unsigned n = 0; n < 16; ++n) {
    p.delay(static_cast<std::uint16_t>(300 + 2000 * n));
    p.request(0x0100);
    p.wait_grant();
    p.w(0x7200); // moveq #0,d1
    p.w(0x1239); // move.b ($A01F01),d1
    p.l(0x00A01F01);
    p.w(0xE149); // lsl.w #8,d1
    p.w(0x1239); // move.b ($A01F00),d1
    p.l(0x00A01F00);
    p.w(0x34C1); // move.w d1,(a2)+
    p.request(0x0000);
  }
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER Z80 CLOCK", payload, p);
}

// `z80_map` (TP-103.2): what the 68000 sees of the Z80 area, with the bus held
// and without it. Every access is logged as a byte (or a word) in work RAM.
inline std::vector<std::uint8_t> z80_map_rom() {
  detail::M68kProgram p;
  p.prologue();
  const auto wb = [&](std::uint32_t addr,
                      std::uint8_t v) { // move.b #v,(addr).l
    p.w(0x13FC);
    p.w(v);
    p.l(addr);
  };
  const auto ww = [&](std::uint32_t addr,
                      std::uint16_t v) { // move.w #v,(addr).l
    p.w(0x33FC);
    p.w(v);
    p.l(addr);
  };
  const auto rb =
      [&](std::uint32_t addr) { // moveq #0,d1 ; move.b (addr).l,d1 ; log
        p.w(0x7200);
        p.w(0x1239);
        p.l(addr);
        p.w(0x34C1);
      };
  const auto rw = [&](std::uint32_t addr) { // move.w (addr).l,d1 ; log
    p.w(0x3239);
    p.l(addr);
    p.w(0x34C1);
  };
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  // Writes: the RAM is 8 KiB mirrored; a word write keeps the high byte.
  wb(0xA00410, 0x11);
  wb(0xA02410, 0x22);
  wb(0xA01FFF, 0x33);
  ww(0xA00420, 0x4455);
  ww(0xA00422, 0x6677);
  wb(0xA00431, 0x88);
  rb(0xA00410);
  rb(0xA02410);
  rb(0xA01FFF);
  rb(0xA03FFF);
  rw(0xA00420);
  rb(0xA00420);
  rb(0xA00421);
  rw(0xA00422);
  rw(0xA00430);
  rb(0xA00431);
  // Mirror of the whole 32 KiB window above $A08000.
  wb(0xA08440, 0x99);
  rb(0xA00440);
  rb(0xA08440);
  wb(0xA0A450, 0x9A);
  rb(0xA00450);
  rb(0xA02450);
  // Reads of the YM2612 ports, the bank register and the VDP area.
  rb(0xA04000);
  rb(0xA04001);
  rb(0xA04002);
  rb(0xA04003);
  rb(0xA05000);
  rb(0xA06000);
  rb(0xA07000);
  rb(0xA07F00);
  rb(0xA07F04);
  rb(0xA07F11);
  rw(0xA04000);
  rw(0xA06000);
  // The control registers read back the request state and open bus bits.
  rb(0xA11100);
  rb(0xA11101);
  rw(0xA11100);
  rb(0xA11200);
  rw(0xA11200);
  // With the bus released the area does not answer the same way.
  p.request(0x0000);
  p.delay(100);
  rb(0xA00410);
  rb(0xA00420);
  rw(0xA11100);
  wb(0xA00460, 0x5A);
  p.request(0x0100);
  p.wait_grant();
  rb(0xA00460);
  rb(0xA00410);
  // Reset asserted: the area is reachable only with the bus.
  p.reset_pin(0x0000);
  rb(0xA00410);
  wb(0xA00461, 0x5B);
  rb(0xA00461);
  p.request(0x0000);
  p.w(0x60FE); // bra.s *
  return detail::z80_rom_image("AYTHER Z80 MAP", {0}, p);
}

// `z80_contention`: the Z80 loops in one of three ways (0 in place, 1 reading
// the ROM through the window, 2 reading its own RAM) while the 68000 samples
// the HV counter between long delays: any 68000 time lost to the Z80 shows up.
inline std::vector<std::uint8_t> z80_contention_rom(unsigned variant) {
  Z80Asm a;
  a.ld_sp(0x1FFF);
  if (variant == 1) {
    a.ld_hl(0x6000);
    a.ld_de(0x0000);
    a.call(kZ80SetBank);
  }
  const std::size_t loop = a.here();
  if (variant == 1)
    a.ld_a_mem(0x8000);
  else if (variant == 2)
    a.ld_a_mem(0x1000);
  a.jr_to(loop);
  a.org(kZ80SetBank);
  z80_setbank_routine(a);
  detail::M68kProgram p;
  p.prologue();
  p.w(0x4DF9); // lea $C00004,a6
  p.l(0x00C00004);
  p.w(0x47F9); // lea $C00008,a3
  p.l(0x00C00008);
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(a.code.size()));
  p.reset_pin(0x0000);
  if (variant != 3) { // 3: the Z80 stays in reset
    p.reset_pin(0x0100);
    p.request(0x0000);
  }
  p.w(0x3CBC); // move.w #..,(a6): mode 5, display on
  p.w(0x8144);
  p.w(0x3CBC); // H40
  p.w(0x8C81);
  for (unsigned n = 1; n <= 6; ++n) {
    p.w(0x3016); // move.w (a6),d0: wait for blanking, then for its end
    p.w(0x0240);
    p.w(0x0008);
    p.w(0x67F8);
    p.w(0x3016);
    p.w(0x0240);
    p.w(0x0008);
    p.w(0x66F8);
    p.delay(static_cast<std::uint16_t>(4000 * n));
    p.w(0x34D3); // move.w (a3),(a2)+
  }
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER Z80 CONT", a.code, p);
}

// `z80_bank_rate` (TP-209.2): the Z80 counts in HL and stores it at $1F00 on
// every pass (inc hl ; [ld a,($8000)] ; ld ($1F00),hl ; jr: 34 T-states, 47
// with the read of the ROM through the window in variant 1). For six lines L
// the 68000 holds the Z80 in reset, waits for the end of the vertical blank,
// releases it, polls the HV counter until line L, takes the bus and logs the
// counter: the Z80 passes in L lines of master time, measured by the VDP and
// not by the 68000, so a 68000 slowed by the Z80 does not bias it.
inline std::vector<std::uint8_t> z80_bank_rate_rom(unsigned variant) {
  Z80Asm a;
  a.ld_sp(0x1FFF);
  if (variant == 1) {
    a.ld_hl(0x6000);
    a.ld_de(0x0000);
    a.call(kZ80SetBank);
  }
  if (variant == 2) { // dense: 4 reads of 29 T-states per count
    a.ld_hl(0x6000);
    a.ld_de(0x0000);
    a.call(kZ80SetBank);
  }
  a.ld_hl(0);
  const std::size_t loop = a.here();
  a.inc_hl();
  if (variant == 1)
    a.ld_a_mem(0x8000);
  if (variant == 2) {
    a.db({0x06, 0x04}); // ld b,4
    const std::size_t inner = a.here();
    a.ld_a_mem(0x8000);
    a.db({0x10, static_cast<std::uint8_t>(static_cast<int>(inner) -
                                          static_cast<int>(a.here() + 2))});
  }
  a.ld_mem_hl(0x1F00);
  a.jr_to(loop);
  a.org(kZ80SetBank);
  z80_setbank_routine(a);
  detail::M68kProgram p;
  p.prologue();
  p.w(0x4DF9); // lea $C00004,a6
  p.l(0x00C00004);
  p.w(0x47F9); // lea $C00008,a3
  p.l(0x00C00008);
  p.w(0x3CBC); // move.w #..,(a6): mode 5, display on
  p.w(0x8144);
  p.w(0x3CBC); // H40
  p.w(0x8C81);
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(a.code.size()));
  p.reset_pin(0x0000);
  p.request(0x0000);
  for (unsigned n = 1; n <= 6; ++n) {
    const auto line = static_cast<std::uint16_t>(32 * n);
    p.reset_pin(0x0000);
    p.w(0x3016); // move.w (a6),d0: wait for blanking, then for its end
    p.w(0x0240);
    p.w(0x0008);
    p.w(0x67F8);
    p.w(0x3016);
    p.w(0x0240);
    p.w(0x0008);
    p.w(0x66F8);
    p.w(0x3013); // the blank flag drops at line $FF: wait for line 0
    p.w(0x0C40); // move.w (a3),d0 ; cmpi.w #$E000,d0 ; bcc.s *-8
    p.w(0xE000);
    p.w(0x64F8);
    p.reset_pin(0x0100); // the Z80 starts
    p.w(0x3013);         // poll: move.w (a3),d0
    p.w(0x0C40);         //       cmpi.w #line<<8,d0
    p.w(static_cast<std::uint16_t>(line << 8));
    p.w(0x65F8); //       bcs.s poll
    p.w(0x34C0); // move.w d0,(a2)+: the HV seen
    p.request(0x0100);
    p.w(0x0814); // btst #0,(a4) ; bne.s *-4: wait for the grant
    p.w(0x0000);
    p.w(0x66FA);
    p.w(0x7200); // moveq #0,d1
    p.w(0x1239); // move.b ($A01F01),d1
    p.l(0x00A01F01);
    p.w(0xE149); // lsl.w #8,d1
    p.w(0x1239); // move.b ($A01F00),d1
    p.l(0x00A01F00);
    p.w(0x34C1); // move.w d1,(a2)+
    p.request(0x0000);
  }
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER Z80 RATE", a.code, p);
}

// `z80_steal` (TP-209.2): how much 68000 time one Z80 access to the 68000 bus
// costs. The Z80 loops on `ld a,($8000)` (ROM through the window, bank 0),
// `nops` NOPs and `jr` (28 + 4 x nops T-states with the 3 wait states);
// `access` 1 makes it a write `ld ($8000),a` (ROM, ignored), 2 a read of its
// own RAM (no 68000 bus), 3 `ld a,$9F ; ld ($7F11),a` (PSG), 4 a read with
// `nops` T-states of filler instead of NOPs, 5 a read every 35 T-states (a
// multiple of 105 master cycles: every access at the same phase of the 68000
// clock) after `nops` T-states of filler that choose that phase, and nops = 99
// keeps the Z80 in reset. For four
// frames the 68000 counts passes of a 30-cycle loop (addq ; btst ; beq) from
// the end of the vertical blank to its start, the 224 active lines, and logs
// the count: fewer passes than with the Z80 in reset is 68000 time taken.
inline std::vector<std::uint8_t> z80_steal_rom(unsigned nops,
                                               unsigned access = 0) {
  Z80Asm a;
  // `t` T-states of filler: ld a,n (7), inc de (6) and nop (4).
  const auto filler = [&](unsigned t) {
    for (unsigned sevens = 0; sevens <= t / 7; ++sevens)
      for (unsigned sixes = 0; sixes <= t / 6; ++sixes) {
        const unsigned used = 7 * sevens + 6 * sixes;
        if (used > t || (t - used) % 4 != 0)
          continue;
        for (unsigned i = 0; i < sevens; ++i)
          a.db({0x3E, 0x00});
        for (unsigned i = 0; i < sixes; ++i)
          a.db({0x13});
        for (unsigned i = 0; i < (t - used) / 4; ++i)
          a.db({0x00});
        return;
      }
  };
  a.ld_sp(0x1FFF);
  a.ld_hl(0x6000);
  a.ld_de(0x0000);
  a.call(kZ80SetBank);
  if (access == 5) // phase: `nops` T-states before a 35 T-state loop
    filler(nops);
  const std::size_t loop = a.here();
  if (access == 1)
    a.db({0x32, 0x00, 0x80}); // ld ($8000),a
  else if (access == 2)
    a.ld_a_mem(0x1000);
  else if (access == 3) {
    a.ld_a(0x9F);       // PSG: channel 0 silent
    a.ld_mem_a(0x7F11); // through the 68000 bus (VDP window)
  } else
    a.ld_a_mem(0x8000);
  if (access == 4) {
    filler(nops);
  } else if (access == 5) {
    filler(7);
  } else {
    for (unsigned i = 0; i < nops && nops != 99; ++i)
      a.db({0x00});
  }
  a.jr_to(loop);
  a.org(kZ80SetBank);
  z80_setbank_routine(a);
  detail::M68kProgram p;
  p.prologue();
  p.w(0x4DF9); // lea $C00004,a6
  p.l(0x00C00004);
  p.w(0x3CBC); // move.w #..,(a6): mode 5, display on
  p.w(0x8144);
  p.w(0x3CBC); // H40
  p.w(0x8C81);
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(a.code.size()));
  p.reset_pin(0x0000);
  if (nops != 99) {
    p.reset_pin(0x0100);
    p.request(0x0000);
  }
  for (unsigned n = 0; n < 4; ++n) {
    p.w(0x3016); // move.w (a6),d0: wait for blanking, then for its end
    p.w(0x0240);
    p.w(0x0008);
    p.w(0x67F8);
    p.w(0x3016);
    p.w(0x0240);
    p.w(0x0008);
    p.w(0x66F8);
    p.w(0x7200); // moveq #0,d1
    p.w(0x5241); // count: addq.w #1,d1          (4)
    p.w(0x082E); //        btst #3,1(a6)         (16, low status byte)
    p.w(0x0003);
    p.w(0x0001);
    p.w(0x67F6); //        beq.s count           (10)
    p.w(0x34C1); // move.w d1,(a2)+
  }
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER Z80 STEAL", a.code, p);
}

// `z80_int` (TP-106.3): the Z80 runs a counter in HL (inc hl ; jr, 18 T-states
// per pass) in interrupt mode 1; its handler (at $0038) logs HL at every entry
// into the Z80 RAM from $1000 on (index at $1F02). The entries per frame and
// their spacing give the INT pulse and its alignment. With `enable_ie` the VDP
// vertical interrupt is not enabled on the 68000 side (its vectors are empty).
inline std::vector<std::uint8_t> z80_int_rom(unsigned variant) {
  Z80Asm a;
  a.ld_sp(0x1FFF);
  a.ld_de(0x1000);
  a.db({0xED, 0x53, 0x02, 0x1F}); // ld ($1F02),de
  a.db({0xED, 0x56});             // im 1
  a.db({0xFB});                   // ei
  a.ld_hl(0);
  const std::size_t loop = a.here();
  a.inc_hl();
  a.jr_to(loop);
  a.org(0x38);
  a.ld_mem_hl(0x1F10);
  a.db({0xF5, 0xD5});             // push af ; push de
  a.db({0xED, 0x5B, 0x02, 0x1F}); // ld de,($1F02)
  a.db({0x7D, 0x12, 0x13, 0x7C, 0x12,
        0x13}); // ld a,l ; ld (de),a ; inc de ; ld a,h ; ld (de),a ; inc de
  a.db({0xED, 0x53, 0x02, 0x1F});       // ld ($1F02),de
  a.db({0xD1, 0xF1, 0xFB, 0xED, 0x4D}); // pop de ; pop af ; ei ; reti
  a.org(kZ80SetBank);
  z80_setbank_routine(a);
  detail::M68kProgram p;
  p.prologue();
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(a.code.size()));
  p.reset_pin(0x0000);
  p.reset_pin(0x0100);
  p.request(0x0000);
  if (variant == 1) { // display and the Z80 interrupt with mode 5 on
    p.w(0x4DF9);
    p.l(0x00C00004);
    p.w(0x3CBC);
    p.w(0x8144);
  }
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER Z80 INT", a.code, p);
}

// `fm_psg_z80` (TP-107.2/107.3): the Z80 writes the YM2612 ($4000-$4005) and
// the PSG ($7F11-$7F19) once, from its own code; compared in frame 0 with the
// AUDIO_WRITES of gpgx.
inline std::vector<std::uint8_t> fm_psg_z80_payload() {
  Z80Asm a;
  const auto put = [&](std::uint16_t address, std::uint8_t value) {
    a.ld_a(value);
    a.ld_mem_a(address);
  };
  put(0x4000, 0x22);
  put(0x4001, 0x08);
  put(0x4000, 0x28);
  put(0x4001, 0xF0);
  put(0x4002, 0x1A);
  put(0x4003, 0x77);
  put(0x4000, 0x2B);
  put(0x4001, 0x80);
  a.ld_mem_a(0x4001); // data again, no address
  put(0x4004, 0x2A);
  put(0x4005, 0x81); // a mirror of the ports
  put(0x4003, 0x66);
  put(0x7F11, 0x9F);
  put(0x7F13, 0x8F);
  put(0x7F15, 0xE4);
  put(0x7F17, 0xBF);
  put(0x7F10, 0x85);
  put(0x7F12, 0x86); // even addresses
  put(0x7F19, 0x87);
  put(0x7F1F, 0x88); // past the PSG ports
  put(0x7F11, 0x91);
  a.jr_self();
  return a.code;
}

inline std::vector<std::uint8_t> fm_psg_z80_rom() {
  const auto payload = fm_psg_z80_payload();
  detail::M68kProgram p;
  p.prologue();
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(payload.size()));
  p.reset_pin(0x0000);
  p.reset_pin(0x0100);
  p.request(0x0000);
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER FM Z80   ", payload, p);
}

// `fm_busy_z80` (TP-107.7): the Z80 writes the YM2612 the way sound drivers do,
// polling the busy flag (bit 7 of $4000) before every address and data write.
// The spacing of the records gives the busy time and the poll loop timing.
inline std::vector<std::uint8_t> fm_busy_z80_payload(unsigned pad = 0) {
  Z80Asm a;
  a.ld_hl(0x4000);
  const auto wait_ready = [&] {
    const auto at = a.here();
    a.db({0xCB, 0x7E}); // bit 7,(hl)
    a.db({0x20, static_cast<std::uint8_t>(static_cast<int>(at) -
                                          static_cast<int>(a.here() + 2))});
  };
  const auto write = [&](std::uint8_t reg, std::uint8_t value) {
    wait_ready();
    a.db({0x36, reg}); // ld (hl),n: address
    a.inc_hl();
    a.db({0x36, value}); // data
    a.db({0x2B});        // dec hl
    // `pad` T-states of filler (nop 4, inc de 6, ld a,n 7) before the next
    // poll, so that the poll lands at another phase of the busy window.
    for (unsigned sevens = 0; sevens <= pad / 7; ++sevens)
      for (unsigned sixes = 0; sixes <= pad / 6; ++sixes) {
        const unsigned used = 7 * sevens + 6 * sixes;
        if (used <= pad && (pad - used) % 4 == 0) {
          for (unsigned i = 0; i < sevens; ++i)
            a.db({0x3E, 0});
          for (unsigned i = 0; i < sixes; ++i)
            a.db({0x13});
          for (unsigned i = 0; i < (pad - used) / 4; ++i)
            a.db({0x00});
          return;
        }
      }
  };
  for (unsigned i = 0; i < 40; ++i)
    write(static_cast<std::uint8_t>(0x30 + i), static_cast<std::uint8_t>(i));
  a.jr_self();
  return a.code;
}

inline std::vector<std::uint8_t> fm_busy_z80_rom(unsigned pad = 0) {
  const auto payload = fm_busy_z80_payload(pad);
  detail::M68kProgram p;
  p.prologue();
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(payload.size()));
  p.reset_pin(0x0000);
  p.reset_pin(0x0100);
  p.request(0x0000);
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER FM BUSY  ", payload, p);
}

// `fm_rom_z80` (TP-107.7): like fm_busy_z80, but every data byte is read from
// the cartridge through the bank window ($8000, bank 0) right before the write,
// as sound drivers do when they stream register data: the Z80 waits for the
// 68000 bus on every read.
inline std::vector<std::uint8_t> fm_rom_z80_payload(unsigned reads) {
  Z80Asm a;
  a.ld_hl(0x4000);
  a.ld_de(0x8100);
  for (unsigned i = 0; i < 40; ++i) {
    for (unsigned k = 0; k < reads; ++k)
      a.db({0x1A, 0x13}); // ld a,(de); inc de
    const auto at = a.here();
    a.db({0xCB, 0x7E}); // bit 7,(hl)
    a.db({0x20, static_cast<std::uint8_t>(static_cast<int>(at) -
                                          static_cast<int>(a.here() + 2))});
    a.db({0x36, static_cast<std::uint8_t>(0x30 + i)});
    a.inc_hl();
    a.db({0x77}); // ld (hl),a
    a.db({0x2B}); // dec hl
  }
  a.jr_self();
  return a.code;
}

inline std::vector<std::uint8_t> fm_rom_z80_rom(unsigned reads) {
  const auto payload = fm_rom_z80_payload(reads);
  detail::M68kProgram p;
  p.prologue();
  p.request(0x0100);
  p.reset_pin(0x0100);
  p.wait_grant();
  p.upload(kPayloadAt, static_cast<std::uint16_t>(payload.size()));
  p.reset_pin(0x0000);
  p.reset_pin(0x0100);
  p.request(0x0000);
  p.w(0x60FE);
  return detail::z80_rom_image("AYTHER FM ROM   ", payload, p);
}

} // namespace tessera::testrom
