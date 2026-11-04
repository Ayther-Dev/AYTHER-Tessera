// TP-105.1 (RF-3.9): frame of the VDP renderer. Per line, RGBA8 output, one
// emission hook per line for phase 2 and no pass after the last line. The
// content is the backdrop colour only; planes and sprites come with TP-105.3+.
// TP-105.5/6: sprites (table, order, priority, X = 0 mask, per line limits and
// the overflow and collision flags).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/vdp.hpp"
#include "md/vdp_render.hpp"
#include <doctest/doctest.h>

#include <vector>

using namespace tessera::md;

namespace {
void reg(Vdp &v, int r, int value) {
  v.write_control(static_cast<std::uint16_t>(0x8000 | (r << 8) | value));
}
void set_colour(Vdp &v, unsigned index, std::uint16_t bgr) {
  reg(v, 15, 2);
  v.write_control(static_cast<std::uint16_t>(0xC000 | (index * 2)));
  v.write_control(0x0000);
  v.write_data(bgr);
}
} // namespace

TEST_CASE("the frame size follows H32/H40 and V28/V30") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04);
  CHECK(frame_size(v).width == 256);
  CHECK(frame_size(v).height == 224);
  reg(v, 12, 0x81);
  CHECK(frame_size(v).width == 320);
  reg(v, 1, 0x0C); // V30
  CHECK(frame_size(v).height == 240);
  reg(v, 12, 0x00);
  CHECK(frame_size(v).width == 256);
  CHECK(frame_size(v).height == 240);
}

TEST_CASE("an empty frame is the backdrop colour in RGBA8") {
  Vdp v(Standard::ntsc);
  reg(v, 0, 0x04); // full palette
  reg(v, 1, 0x04);
  set_colour(v, 3, 0x0E00); // blue = 7, green = 0, red = 0
  reg(v, 7, 0x03);          // backdrop = palette 0, colour 3
  Renderer r;
  r.begin_frame(v);
  for (std::uint32_t line = 0; line < 224; ++line)
    r.render_line(v, line);
  REQUIRE(r.frame().size() == 256U * 224U);
  const std::uint32_t pixel = r.frame()[0];
  CHECK((pixel & 0xFF) == 0x00);         // red
  CHECK(((pixel >> 8) & 0xFF) == 0x00);  // green
  CHECK(((pixel >> 16) & 0xFF) == 0xEF); // blue 7 -> 565 level 29 -> 8 bits
  CHECK((pixel >> 24) == 0xFF);          // alpha
  for (const auto p : r.frame())
    CHECK(p == pixel);
}

TEST_CASE("register 0 bit 2 clear limits each channel to its lowest bit") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04);
  set_colour(v, 1, 0x0EEE);
  reg(v, 7, 0x01);
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  const std::uint32_t limited = r.frame()[0];
  CHECK((limited & 0xFF) == 0x21); // level 1 -> 565 level 4 -> 8 bits
  reg(v, 0, 0x04);
  r.render_line(v, 0);
  CHECK((r.frame()[0] & 0xFF) == 0xEF); // full palette
}

TEST_CASE("the backdrop can change between lines and the hook sees each line") {
  Vdp v(Standard::ntsc);
  reg(v, 0, 0x04); // full palette
  reg(v, 1, 0x04);
  set_colour(v, 1, 0x000E); // red
  set_colour(v, 2, 0x00E0); // green
  Renderer r;
  std::vector<std::uint32_t> hooked_lines;
  std::vector<std::uint32_t> first_pixels;
  r.set_line_hook([&](std::uint32_t line, std::span<const std::uint32_t> row) {
    hooked_lines.push_back(line);
    first_pixels.push_back(row[0]);
    CHECK(row.size() == 256);
  });
  r.begin_frame(v);
  reg(v, 7, 0x01);
  r.render_line(v, 0);
  reg(v, 7, 0x02);
  r.render_line(v, 1);
  REQUIRE(hooked_lines.size() == 2);
  CHECK(hooked_lines[0] == 0);
  CHECK(hooked_lines[1] == 1);
  CHECK((first_pixels[0] & 0xFF) == 0xEF);
  CHECK(((first_pixels[1] >> 8) & 0xFF) == 0xEF);
  // no pass after the lines: the frame already holds what the hook saw
  CHECK(r.frame()[0] == first_pixels[0]);
  CHECK(r.frame()[256] == first_pixels[1]);
}

TEST_CASE("lines outside the active display are not drawn") {
  Vdp v(Standard::ntsc);
  reg(v, 1, 0x04);
  Renderer r;
  unsigned calls = 0;
  r.set_line_hook(
      [&](std::uint32_t, std::span<const std::uint32_t>) { ++calls; });
  r.begin_frame(v);
  r.render_line(v, 224);
  r.render_line(v, 300);
  CHECK(calls == 0);
}

namespace {
// Plane B at $C000 (reg 4 = 6), 32x32, tiles from $0000.
void plane_b_scene(Vdp &v) {
  reg(v, 0, 0x04);
  reg(v, 1, 0x44);
  reg(v, 2, 0x10);
  reg(v, 4, 0x06);
  reg(v, 15, 2);
  set_colour(v, 1, 0x000E); // palette 0, colour 1: red
  set_colour(v, 2, 0x00E0); // palette 0, colour 2: green
  // tile 1: first pixel colour 1, the rest colour 2 (row 0 only)
  v.write_control(0x4020);
  v.write_control(0x0000);
  v.write_data(0x1222);
  v.write_data(0x2222);
  // name table entry (0,0) = tile 1
  v.write_control(0x4000);
  v.write_control(0x0003);
  v.write_data(0x0001);
}
} // namespace

TEST_CASE("plane B draws a tile pixel with its palette colour") {
  Vdp v(Standard::ntsc);
  plane_b_scene(v);
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((r.frame()[0] & 0xFF) == 0xEF);        // red
  CHECK(((r.frame()[1] >> 8) & 0xFF) == 0xEF); // green
}

TEST_CASE("a horizontal flip mirrors the tile pixels") {
  Vdp v(Standard::ntsc);
  plane_b_scene(v);
  v.write_control(0x4000);
  v.write_control(0x0003);
  v.write_data(0x0801); // tile 1 with horizontal flip
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((r.frame()[7] & 0xFF) == 0xEF); // red now on the right
}

TEST_CASE("full horizontal scroll moves the plane to the right") {
  Vdp v(Standard::ntsc);
  plane_b_scene(v);
  reg(v, 13, 0x38);
  v.write_control(0x6000); // VRAM write at $E000
  v.write_control(0x0003);
  v.write_data(0x0000); // plane A scroll
  v.write_data(0x0003); // plane B scroll 3
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((r.frame()[3] & 0xFF) == 0xEF); // red pixel moved from x 0 to x 3
}

TEST_CASE("shadow mode darkens a pixel without the priority bit") {
  Vdp v(Standard::ntsc);
  plane_b_scene(v);
  reg(v, 12, 0x08);
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((r.frame()[0] & 0xFF) == 0x73); // level 7 as shadow: 14 -> 8 bits
}

namespace {
// Sprite table at $E000 (register 5 = $70). Entry n: y, size and link,
// attributes, x (all raw values, 128 added to the positions).
void sprite(Vdp &v, unsigned n, int x, int y, unsigned size_link,
            unsigned attr) {
  const unsigned at = 0xE000U + n * 8U;
  v.write_control(static_cast<std::uint16_t>(0x4000 | (at & 0x3FFF)));
  v.write_control(static_cast<std::uint16_t>((at >> 14) & 3));
  v.write_data(static_cast<std::uint16_t>(y + 128));
  v.write_data(static_cast<std::uint16_t>(size_link));
  v.write_data(static_cast<std::uint16_t>(attr));
  v.write_data(static_cast<std::uint16_t>(x == -128 ? 0 : x + 128));
}
void sprite_scene(Vdp &v) {
  reg(v, 0, 0x04);
  reg(v, 1, 0x44);
  reg(v, 5, 0x70);
  reg(v, 15, 2);
  set_colour(v, 1, 0x000E);
  set_colour(v, 2, 0x00E0);
  // tile 1: all pixels colour 1, tile 2: all pixels colour 2 (8 rows each)
  v.write_control(0x4020);
  v.write_control(0x0000);
  for (unsigned i = 0; i < 8; ++i) {
    v.write_data(0x1111);
    v.write_data(0x1111);
  }
  for (unsigned i = 0; i < 8; ++i) {
    v.write_data(0x2222);
    v.write_data(0x2222);
  }
}
std::uint32_t pixel_at(Renderer &r, unsigned x) { return r.frame()[x]; }
} // namespace

TEST_CASE("a sprite is drawn at its position with its palette colour") {
  Vdp v(Standard::ntsc);
  sprite_scene(v);
  sprite(v, 0, 10, 0, 0x0000, 1); // 1x1 cell, link 0
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((pixel_at(r, 10) & 0xFF) == 0xEF);
  CHECK((pixel_at(r, 17) & 0xFF) == 0xEF);
  CHECK((pixel_at(r, 9) & 0xFF) != 0xEF);
  CHECK((pixel_at(r, 18) & 0xFF) != 0xEF);
  CHECK(!r.take_sprite_flags().collision);
}

TEST_CASE("the first sprite of the list is on top and overlap is a collision") {
  Vdp v(Standard::ntsc);
  sprite_scene(v);
  sprite(v, 0, 10, 0, 0x0001, 1); // link to 1
  sprite(v, 1, 14, 0, 0x0000, 2);
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((pixel_at(r, 15) & 0xFF) == 0xEF);        // sprite 0 over sprite 1
  CHECK(((pixel_at(r, 20) >> 8) & 0xFF) == 0xEF); // sprite 1 beyond sprite 0
  const auto flags = r.take_sprite_flags();
  CHECK(flags.collision);
  CHECK(!flags.overflow);
  CHECK(!r.take_sprite_flags().collision); // reading clears
}

TEST_CASE(
    "X = 0 masks the sprites after it once another sprite is on the line") {
  Vdp v(Standard::ntsc);
  sprite_scene(v);
  sprite(v, 0, 10, 0, 0x0001, 1);
  sprite(v, 1, -128, 0, 0x0002, 1); // raw X = 0
  sprite(v, 2, 30, 0, 0x0000, 2);   // masked
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK((pixel_at(r, 10) & 0xFF) == 0xEF);
  CHECK(((pixel_at(r, 30) >> 8) & 0xFF) != 0xEF);
}

TEST_CASE("a seventeenth sprite on a line sets overflow in H32") {
  Vdp v(Standard::ntsc);
  sprite_scene(v);
  for (unsigned n = 0; n < 17; ++n)
    sprite(v, n, static_cast<int>(n) * 10, 0, n == 16 ? 0x0000 : n + 1, 1);
  Renderer r;
  r.begin_frame(v);
  r.render_line(v, 0);
  CHECK(r.take_sprite_flags().overflow);
  CHECK((pixel_at(r, 15 * 10) & 0xFF) == 0xEF); // sixteenth drawn
  CHECK((pixel_at(r, 16 * 10) & 0xFF) != 0xEF); // seventeenth dropped
}
