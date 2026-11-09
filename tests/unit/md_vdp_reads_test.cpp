// TP-104.5 (RF-2.2): reads of the VDP by the 68000 through the bus, and the
// HV counter latch.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "md/vdp.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

using namespace tessera::md;

namespace {
System make_system() {
  System s(tessera::testrom::vint_counter_rom(),
           MachineConfig::for_region(Region::usa));
  s.reset();
  return s;
}
// Command to the control port through the 68000 bus.
void command(System &s, std::uint16_t first, std::uint16_t second) {
  s.write16(0xC00004, first);
  s.write16(0xC00004, second);
}
} // namespace

TEST_CASE(
    "VRAM written by the 68000 is read back word by word and byte by byte") {
  System s = make_system();
  s.write16(0xC00004, 0x8104); // mode 5, so that register 15 exists
  s.write16(0xC00004, 0x8F02); // increment 2
  command(s, 0x4020, 0x0000);  // VRAM write at $0020
  s.write16(0xC00000, 0x1234);
  s.write16(0xC00000, 0x5678);
  command(s, 0x0020, 0x0000); // VRAM read at $0020
  CHECK(s.read16(0xC00000) == 0x1234);
  CHECK(s.read16(0xC00000) == 0x5678);
  command(s, 0x0020, 0x0000);
  CHECK(s.read8(0xC00000) == 0x12); // high byte of the word
  command(s, 0x0020, 0x0000);
  CHECK(s.read8(0xC00001) == 0x34); // low byte of the same word
}

TEST_CASE("byte writes to the data port repeat the byte in both halves") {
  System s = make_system();
  s.write16(0xC00004, 0x8104); // mode 5, so that register 15 exists
  s.write16(0xC00004, 0x8F02);
  command(s, 0x4040, 0x0000);
  s.write8(0xC00000, 0xAB);
  command(s, 0x0040, 0x0000);
  CHECK(s.read16(0xC00000) == 0xABAB);
}

TEST_CASE("CRAM and VSRAM read commands through the bus") {
  System s = make_system();
  s.write16(0xC00004, 0x8104); // mode 5, so that register 15 exists
  s.write16(0xC00004, 0x8F02);
  command(s, 0xC006, 0x0000); // CRAM write at 6
  s.write16(0xC00000, 0x0ACE);
  command(s, 0x0006, 0x0020); // CRAM read at 6 (code 8)
  CHECK(s.read16(0xC00000) == 0x0ACE);
  command(s, 0x4002, 0x0010); // VSRAM write at 2 (code 5)
  s.write16(0xC00000, 0x0155);
  command(s, 0x0002, 0x0010); // code 4: VSRAM read
  CHECK(s.read16(0xC00000) == 0x0155);
}

TEST_CASE("status read through the bus: word and the two bytes") {
  System s = make_system();
  const std::uint16_t word = s.read16(0xC00004);
  // FIFO empty, display off = blanking; bits 15-10 are the open bus, the
  // word the 68000 has prefetched (measured on gpgx, TP-110.4)
  CHECK((word & 0x03FF) == 0x0208);
  CHECK((word & 0xFC00) == (s.cpu().getIRC() & 0xFC00));
  CHECK(s.read8(0xC00004) == (word >> 8));
  CHECK(s.read8(0xC00005) == (word & 0xFF));
  // the mirror of the status port answers the same
  CHECK(s.read16(0xC00006) == word);
}

TEST_CASE("HV counter is readable at C00008 and mirrors") {
  System s = make_system();
  const std::uint16_t hv = s.read16(0xC00008);
  CHECK((hv >> 8) == 0); // line 0 before the first frame runs
  CHECK(s.read16(0xC0000A) == hv);
  CHECK(s.read8(0xC00008) == (hv >> 8));
}

TEST_CASE("HV latch: frozen while the latch is enabled, live otherwise") {
  Vdp v(Standard::ntsc);
  v.begin_line(50);
  v.write_control(0x8000 | 0x02); // register 0, bit 1: latch enable
  v.latch_hv(1700);
  const auto latched = v.read_hv(1700);
  v.begin_line(120);
  CHECK(v.read_hv(3000) == latched); // still the latched value
  CHECK((latched >> 8) == 50);
  v.write_control(0x8000); // latch disabled: live counter again
  CHECK((v.read_hv(3000) >> 8) == 120);
}

TEST_CASE("without the enable bit a latch request is ignored") {
  Vdp v(Standard::ntsc);
  v.begin_line(50);
  v.latch_hv(1700);
  v.begin_line(60);
  CHECK((v.read_hv(100) >> 8) == 60);
}

TEST_CASE("the TH line falling on port 2 latches the HV counter") {
  System s = make_system();
  s.write16(0xC00004, 0x8002); // latch enable
  s.write8(0xA1000B, 0x40);    // port 2: TH is an output
  s.write8(0xA10005, 0x40);    // TH high
  s.write8(0xA10005, 0x00);    // TH falls: latch
  const auto latched = s.read16(0xC00008);
  CHECK(s.read16(0xC00008) == latched);
}
