// TP-104.2 (RF-2.2): one test per VDP register, 0 to 23. Each case sets the
// register to two values and checks the effect that tells them apart on a
// minimal scene (renderer, interrupts or DMA). Registers that have no effect
// on a Mega Drive with 64 KiB of VRAM (6, 8, 9, 14) are checked to be kept
// and to leave the frame unchanged. Other files test the same registers in
// more depth; this one is the inventory.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/vdp.hpp"
#include "md/vdp_render.hpp"
#include <doctest/doctest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

using namespace tessera::md;

namespace {
void reg(Vdp &v, int r, int value) {
  v.write_control(static_cast<std::uint16_t>(0x8000 | (r << 8) | value));
}
void write_vram(Vdp &v, unsigned addr, std::initializer_list<int> words) {
  reg(v, 15, 2);
  v.write_control(static_cast<std::uint16_t>(0x4000 | (addr & 0x3FFF)));
  v.write_control(static_cast<std::uint16_t>((addr >> 14) & 3));
  for (const int w : words)
    v.write_data(static_cast<std::uint16_t>(w));
}
void set_colour(Vdp &v, unsigned index, std::uint16_t bgr) {
  reg(v, 15, 2);
  v.write_control(static_cast<std::uint16_t>(0xC000 | (index * 2)));
  v.write_control(0x0000);
  v.write_data(bgr);
}

constexpr std::uint32_t kRed = 0xEF;   // colour 0x00E red channel
constexpr std::uint32_t kGreen = 0xEF; // colour 0x0E0 green channel
bool red(std::uint32_t p) { return (p & 0xFF) == kRed; }
bool green(std::uint32_t p) { return ((p >> 8) & 0xFF) == kGreen; }

/// Mode 5, display on, full palette; colours 1 red, 2 green, backdrop 0
/// (black). Tile 1: all pixels colour 1; tile 2: all pixels colour 2. Plane A
/// at $C000, plane B at $E000, window at $D000, sprites at $F000 (empty
/// unless a case writes it), horizontal scroll at $FC00, all zero.
void scene(Vdp &v) {
  reg(v, 0, 0x04);
  reg(v, 1, 0x44);
  reg(v, 2, 0x30); // plane A $C000
  reg(v, 3, 0x34); // window $D000
  reg(v, 4, 0x07); // plane B $E000
  reg(v, 5, 0x78); // sprites $F000
  reg(v, 13, 0x3F);
  set_colour(v, 1, 0x000E);
  set_colour(v, 2, 0x00E0);
  for (unsigned tile = 1; tile <= 2; ++tile) {
    const int fill = tile == 1 ? 0x1111 : 0x2222;
    write_vram(v, tile * 32,
               {fill, fill, fill, fill, fill, fill, fill, fill, fill, fill,
                fill, fill, fill, fill, fill, fill});
  }
}

std::vector<std::uint32_t> draw(Vdp &v, std::uint32_t line = 0) {
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, line);
  const auto width = frame_size(v).width;
  return {r.frame().begin() + line * width,
          r.frame().begin() + (line + 1) * width};
}
} // namespace

TEST_CASE("register 0: bit 4 enables the horizontal interrupt") {
  for (const int value : {0x04, 0x14}) {
    Vdp v(Standard::ntsc);
    reg(v, 0, value);
    reg(v, 10, 0); // every line
    v.begin_line(0);
    v.begin_line(1);
    CHECK(v.irq_level() == (value == 0x14 ? 4 : 0));
  }
}

TEST_CASE("register 1: bit 6 blanks the display to the backdrop") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xC000, {0x0001});
  CHECK(red(draw(v)[0]));
  reg(v, 1, 0x04); // display off
  CHECK(!red(draw(v)[0]));
}

TEST_CASE("register 2: the plane A name table base") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xC000, {0x0001});
  CHECK(red(draw(v)[0]));
  reg(v, 2, 0x28); // $A000, empty
  CHECK(!red(draw(v)[0]));
}

TEST_CASE("register 3: the window name table base") {
  Vdp v(Standard::ntsc);
  scene(v);
  reg(v, 18, 0x80); // window from line 0 down: every line
  write_vram(v, 0xD000, {0x0001});
  CHECK(red(draw(v)[0]));
  reg(v, 3, 0x28); // $A000, empty
  CHECK(!red(draw(v)[0]));
}

TEST_CASE("register 4: the plane B name table base") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xE000, {0x0002});
  CHECK(green(draw(v)[0]));
  reg(v, 4, 0x05); // $A000, empty
  CHECK(!green(draw(v)[0]));
}

TEST_CASE("register 5: the sprite attribute table base") {
  Vdp v(Standard::ntsc);
  scene(v);
  // sprite 0 at (10, 0), 1x1 cell, link 0, tile 1
  write_vram(v, 0xF000, {128, 0x0000, 0x0001, 10 + 128});
  CHECK(red(draw(v)[10]));
  reg(v, 5, 0x70); // $E000: plane B, all zero, no sprite on line 0
  CHECK(!red(draw(v)[10]));
}

TEST_CASE("register 6: the 128 KiB sprite bit has no effect with 64 KiB") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xF000, {128, 0x0000, 0x0001, 10 + 128});
  const auto before = draw(v);
  reg(v, 6, 0x01);
  CHECK(v.reg(6) == 0x01);
  CHECK(draw(v) == before);
}

TEST_CASE("register 7: the backdrop palette and colour") {
  Vdp v(Standard::ntsc);
  scene(v);
  reg(v, 7, 0x01);
  CHECK(red(draw(v)[0]));
  reg(v, 7, 0x02);
  CHECK(green(draw(v)[0]));
  set_colour(v, 0x21, 0x000E); // palette 2, colour 1
  reg(v, 7, 0x21);
  CHECK(red(draw(v)[0]));
}

TEST_CASE("registers 8 and 9: Mode 4 scroll, no effect in Mode 5") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xC000, {0x0001});
  const auto before = draw(v);
  reg(v, 8, 0x10);
  reg(v, 9, 0x10);
  CHECK(v.reg(8) == 0x10);
  CHECK(v.reg(9) == 0x10);
  CHECK(draw(v) == before);
}

TEST_CASE("register 10: the horizontal interrupt line counter") {
  for (const int every : {1, 4}) {
    Vdp v(Standard::ntsc);
    reg(v, 0, 0x14);
    reg(v, 10, every - 1);
    unsigned fired = 0;
    for (std::uint32_t line = 0; line < 8; ++line) {
      v.begin_line(line);
      if (v.irq_level() == 4) {
        ++fired;
        v.acknowledge(4);
      }
    }
    CHECK(fired == 8U / static_cast<unsigned>(every));
  }
}

TEST_CASE("register 11: the horizontal scroll mode (full or per line)") {
  Vdp v(Standard::ntsc);
  scene(v);
  reg(v, 13, 0x3F);                // scroll table $FC00
  write_vram(v, 0xC000, {0x0001}); // plane A (0,0): tile 1
  // line 0: plane A scroll 0; line 1 (per line mode): plane A scroll 3
  write_vram(v, 0xFC00, {0x0000, 0x0000, 0x0003, 0x0000});
  reg(v, 11, 0x00); // full screen: line 1 uses the entry of line 0
  CHECK(red(draw(v, 1)[0]));
  reg(v, 11, 0x03); // per line: line 1 moves 3 pixels right
  CHECK(!red(draw(v, 1)[0]));
  CHECK(red(draw(v, 1)[3]));
  CHECK(red(draw(v, 0)[0]));
}

TEST_CASE("register 12: H40 widens the line to 320 pixels") {
  Vdp v(Standard::ntsc);
  scene(v);
  reg(v, 16, 0x01);                         // plane A 64 cells wide
  write_vram(v, 0xC000 + 39 * 2, {0x0001}); // column 39, only seen in H40
  CHECK(draw(v).size() == 256U);
  reg(v, 12, 0x81);
  const auto line = draw(v);
  REQUIRE(line.size() == 320U);
  CHECK(red(line[39 * 8]));
}

TEST_CASE("register 13: the horizontal scroll table base") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xC000, {0x0001});
  write_vram(v, 0xB800, {0x0003}); // plane A scroll 3 in the table at $B800
  reg(v, 13, 0x3F);                // $FC00: zero
  CHECK(red(draw(v)[0]));
  reg(v, 13, 0x2E); // $B800
  CHECK(!red(draw(v)[0]));
  CHECK(red(draw(v)[3]));
}

TEST_CASE("register 14: the 128 KiB plane bits have no effect with 64 KiB") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xC000, {0x0001});
  const auto before = draw(v);
  reg(v, 14, 0x11);
  CHECK(v.reg(14) == 0x11);
  CHECK(draw(v) == before);
}

TEST_CASE("register 15: the auto-increment") {
  for (const int step : {2, 4}) {
    Vdp v(Standard::ntsc);
    reg(v, 1, 0x04);
    reg(v, 15, step);
    v.write_control(0x4100);
    v.write_control(0x0000);
    v.write_data(0x1122);
    v.write_data(0x3344);
    CHECK(v.vram()[0x100 + step] == 0x33);
    CHECK(v.vram()[0x100 + step + 1] == 0x44);
  }
}

TEST_CASE("register 16: the plane size sets the row stride") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xC000 + 64 * 2, {0x0001}); // entry 64
  reg(v, 16, 0x00);                         // 32x32: entry 64 is row 2, line 16
  CHECK(red(draw(v, 16)[0]));
  CHECK(!red(draw(v, 8)[0]));
  reg(v, 16, 0x01); // 64x32: entry 64 is row 1, line 8
  CHECK(red(draw(v, 8)[0]));
  CHECK(!red(draw(v, 16)[0]));
}

TEST_CASE("register 17: the window horizontal split") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xD000, {0x0001, 0x0001, 0x0001, 0x0001}); // window row 0
  write_vram(v, 0xC000, {0x0002, 0x0002, 0x0002, 0x0002}); // plane A row 0
  reg(v, 17, 0x01); // window left of cell 2 (16 pixels)
  auto line = draw(v);
  CHECK(red(line[0]));
  CHECK(green(line[16]));
  reg(v, 17, 0x81); // window right of cell 2
  line = draw(v);
  CHECK(green(line[0]));
  CHECK(red(line[16]));
}

TEST_CASE("register 18: the window vertical split") {
  Vdp v(Standard::ntsc);
  scene(v);
  write_vram(v, 0xD000, {0x0001});      // window (0,0)
  write_vram(v, 0xD000 + 64, {0x0001}); // window (0,1), 32 cells wide
  write_vram(v, 0xC000, {0x0002});      // plane A (0,0)
  write_vram(v, 0xC000 + 64, {0x0002}); // plane A (0,1)
  reg(v, 18, 0x01);                     // window above line 8
  CHECK(red(draw(v, 0)[0]));
  CHECK(green(draw(v, 8)[0]));
  reg(v, 18, 0x81); // window from line 8 down
  CHECK(green(draw(v, 0)[0]));
  CHECK(red(draw(v, 8)[0]));
}

TEST_CASE("registers 19 and 20: the DMA length in words") {
  for (const unsigned length : {2U, 0x102U}) {
    Vdp v(Standard::ntsc);
    unsigned reads = 0;
    v.set_dma_reader([&](std::uint32_t) -> std::uint16_t {
      ++reads;
      return 0x5555;
    });
    reg(v, 1, 0x14);
    reg(v, 15, 2);
    reg(v, 19, static_cast<int>(length & 0xFF));
    reg(v, 20, static_cast<int>(length >> 8));
    reg(v, 21, 0);
    reg(v, 22, 0);
    reg(v, 23, 0);
    v.write_control(0x4000);
    v.write_control(0x0080); // VRAM write with the DMA bit
    CHECK(reads == length);
  }
}

TEST_CASE("registers 21 to 23: the DMA source and mode") {
  Vdp v(Standard::ntsc);
  std::vector<std::uint32_t> addresses;
  v.set_dma_reader([&](std::uint32_t address) -> std::uint16_t {
    addresses.push_back(address);
    return 0x1234;
  });
  reg(v, 1, 0x14);
  reg(v, 15, 2);
  reg(v, 19, 1);
  reg(v, 20, 0);
  reg(v, 21, 0x34); // source / 2 = $1A1234 -> source $342468
  reg(v, 22, 0x12);
  reg(v, 23, 0x1A); // bits 7-6 = 0: 68000 to VDP
  v.write_control(0x4000);
  v.write_control(0x0080);
  REQUIRE(addresses.size() == 1);
  CHECK(addresses[0] == 0x342468U);
  // bits 7-6 = 10: fill from the next data write, no bus read
  addresses.clear();
  reg(v, 1, 0x14);
  reg(v, 15, 1);
  reg(v, 19, 4);
  reg(v, 23, 0x80);
  v.write_control(0x4200);
  v.write_control(0x0080);
  v.write_data(0xABAB);
  CHECK(addresses.empty());
  CHECK(v.vram()[0x203] == 0xAB);
}
