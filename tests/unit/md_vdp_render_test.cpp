// TP-105.1 (RF-3.9): frame of the VDP renderer. Per line, RGBA8 output, one
// emission hook per line for phase 2 and no pass after the last line. The
// content is the backdrop colour only; planes and sprites come with TP-105.3+.
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
  CHECK(((pixel >> 16) & 0xFF) == 0xFF); // blue, scaled to 8 bits
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
  CHECK((limited & 0xFF) == 0x24); // level 1 of 7 -> 36
  reg(v, 0, 0x04);
  r.render_line(v, 0);
  CHECK((r.frame()[0] & 0xFF) == 0xFF); // full palette
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
  CHECK((first_pixels[0] & 0xFF) == 0xFF);
  CHECK(((first_pixels[1] >> 8) & 0xFF) == 0xFF);
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
