#include "md/vdp_render.hpp"

#include <algorithm>

namespace tessera::md {

FrameSize frame_size(const Vdp &vdp) noexcept {
  FrameSize size;
  size.width = (vdp.reg(12) & 0x81U) != 0 ? 320U : 256U;
  size.height = (vdp.reg(1) & 0x08U) != 0 ? 240U : 224U;
  return size;
}

std::uint16_t colour_to_565(std::uint16_t cram, Shade shade) noexcept {
  // Measured on gpgx (palette_sweep, all 512 colours, and its PALETTE region
  // for the highlight): the 3 bit level is widened to a 4 bit level L (normal
  // 2*level, shadow level, highlight level + 7), then to the 565 output by
  // bit replication (5 bits (L<<1)|(L>>3), 6 bits (L<<2)|(L>>2)).
  const auto expand = [shade](unsigned level, unsigned bits) {
    const unsigned l4 = shade == Shade::normal   ? level * 2U
                        : shade == Shade::shadow ? level
                                                 : level + 7U;
    return bits == 5 ? ((l4 << 1) | (l4 >> 3)) : ((l4 << 2) | (l4 >> 2));
  };
  const unsigned r5 = expand((cram >> 1) & 7U, 5);
  const unsigned g6 = expand((cram >> 5) & 7U, 6);
  const unsigned b5 = expand((cram >> 9) & 7U, 5);
  return static_cast<std::uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

std::uint32_t colour_to_rgba(std::uint16_t cram, Shade shade) noexcept {
  const std::uint32_t c = colour_to_565(cram, shade);
  const std::uint32_t r5 = c >> 11, g6 = (c >> 5) & 63U, b5 = c & 31U;
  // a host expands the 565 back to 8 bits
  const std::uint32_t red = (r5 << 3) | (r5 >> 2);
  const std::uint32_t green = (g6 << 2) | (g6 >> 4);
  const std::uint32_t blue = (b5 << 3) | (b5 >> 2);
  return red | (green << 8) | (blue << 16) | 0xFF000000U;
}

std::array<std::uint16_t, 256> palette_565(const Vdp &vdp) noexcept {
  // PALETTE region: shadow, normal and highlight tables of the 64 CRAM
  // entries, then 64 zero entries (layout read from gpgx r4).
  std::array<std::uint16_t, 256> table{};
  const bool limited = (vdp.reg(0) & 0x04U) == 0;
  // Measured on gpgx: entry 0 holds the backdrop colour (register 7), the
  // transparent entry of palettes 1 to 3 is black, and without shadow and
  // highlight (register 12 bit 3) all three tables hold the normal colour.
  const bool shade = (vdp.reg(12) & 0x08U) != 0;
  for (unsigned i = 0; i < 64U; ++i) {
    std::uint16_t value = vdp.cram()[i];
    if (i == 0)
      value = vdp.cram()[vdp.reg(7) & 0x3FU];
    else if ((i & 15U) == 0)
      value = 0;
    if (limited)
      value &= 0x0222U;
    table[64U + i] = colour_to_565(value, Shade::normal);
    table[i] = shade ? colour_to_565(value, Shade::shadow) : table[64U + i];
    table[128U + i] =
        shade ? colour_to_565(value, Shade::highlight) : table[64U + i];
  }
  return table;
}

void Renderer::begin_frame(const Vdp &vdp) {
  size_ = frame_size(vdp);
  field_ ^= 1U;
  double_res_ = (vdp.reg(12) & 0x06U) == 0x06U;
  frame_.assign(std::size_t{size_.width} * size_.height, 0xFF000000U);
}

unsigned Renderer::hscroll_value(const Vdp &vdp, unsigned plane,
                                 std::uint32_t line) const {
  // Horizontal scroll: entry per 4 bytes, plane A then plane B.
  const std::uint32_t hs_base = std::uint32_t{vdp.reg(13) & 0x3FU} << 10;
  std::uint32_t hs_entry = 0;
  switch (vdp.reg(11) & 3U) {
  case 0:
    hs_entry = 0;
    break;
  case 1:
    hs_entry = (line & 7U) * 4U; // invalid mode: the first 8 entries repeat
    break;
  case 2:
    hs_entry = (line & ~7U) * 4U;
    break;
  default:
    hs_entry = line * 4U;
    break;
  }
  const std::uint32_t at = (hs_base + hs_entry + plane * 2U) & 0xFFFEU;
  const auto &vram = vdp.vram();
  return ((std::uint32_t{vram[at]} << 8) | vram[at + 1U]) & 0x3FFU;
}

void Renderer::plane_line(const Vdp &vdp, unsigned plane, std::uint32_t line,
                          std::span<Pixel> out) const {
  const unsigned columns = static_cast<unsigned>(out.size()) / 8U;
  const std::uint8_t size_reg = vdp.reg(16);
  const unsigned cells_x = (size_reg & 3U) == 0   ? 32U
                           : (size_reg & 3U) == 1 ? 64U
                                                  : 128U;
  const unsigned cells_y = ((size_reg >> 4) & 3U) == 0   ? 32U
                           : ((size_reg >> 4) & 3U) == 1 ? 64U
                                                         : 128U;
  const std::uint32_t table = plane == 0
                                  ? (std::uint32_t{vdp.reg(2) & 0x38U} << 10)
                                  : (std::uint32_t{vdp.reg(4) & 0x07U} << 13);
  const auto &vram = vdp.vram();
  const unsigned hscroll = hscroll_value(vdp, plane, line);
  const bool two_cell_v = (vdp.reg(11) & 0x04U) != 0;
  const unsigned pixels_x = cells_x * 8U;
  const unsigned cell_h = double_res_ ? 16U : 8U;
  const unsigned pixels_y = cells_y * cell_h;
  const unsigned dline = double_res_ ? line * 2U + field_ : line;
  const unsigned vmask = double_res_ ? 0x7FFU : 0x3FFU;
  for (unsigned x = 0; x < columns * 8U; ++x) {
    // The 2-cell vertical scroll columns are 16 pixel bands whose phase
    // follows the horizontal scroll (measured on gpgx): band edges sit at
    // x = (hscroll & 15) + 16 n; left of the first edge a partial band.
    const unsigned phase = hscroll & 15U;
    unsigned vscroll = 0;
    if (!two_cell_v) {
      vscroll = vdp.vsram()[plane] & vmask;
    } else {
      // The partial band left of the first edge scrolls by 0 in H32 and by
      // the AND of the last pair in H40 (measured).
      const int pair = static_cast<int>((x + 16U - phase) / 16U) - 1;
      if (pair >= 0 && pair < 20)
        vscroll = vdp.vsram()[static_cast<unsigned>(pair) * 2U + plane] & vmask;
      else if (pair < 0 && columns == 40U)
        vscroll = (vdp.vsram()[38] & vdp.vsram()[39]) & vmask;
    }
    // Left window: for the first (hscroll & 15) pixels after the boundary
    // plane A shows the tiles two columns on (measured on gpgx).
    unsigned skew = 0;
    if (plane == 0 && (vdp.reg(17) & 0x80U) == 0 &&
        (vdp.reg(17) & 0x1FU) != 0) {
      const unsigned edge = (vdp.reg(17) & 0x1FU) * 16U;
      if (x >= edge && x < edge + (hscroll & 15U))
        skew = 16U;
    }
    const unsigned px = (x + skew + pixels_x - (hscroll % pixels_x)) % pixels_x;
    const unsigned py = (dline + vscroll) % pixels_y;
    const unsigned cell_x = px / 8U;
    const unsigned cell_y = py / cell_h;
    const std::uint32_t entry_at =
        (table + (cell_y * cells_x + cell_x) * 2U) & 0xFFFFU;
    const std::uint16_t entry = static_cast<std::uint16_t>(
        (std::uint32_t{vram[entry_at]} << 8) | vram[entry_at + 1U]);
    unsigned tile_x = px & 7U;
    unsigned tile_y = py % cell_h;
    if (entry & 0x0800U)
      tile_x = 7U - tile_x;
    if (entry & 0x1000U)
      tile_y = cell_h - 1U - tile_y;
    const std::uint32_t tile_at =
        ((std::uint32_t{entry & 0x7FFU} * cell_h * 4U) + tile_y * 4U +
         tile_x / 2U) &
        0xFFFFU;
    const std::uint8_t byte = vram[tile_at];
    const unsigned colour = (tile_x & 1U) ? (byte & 0x0FU) : (byte >> 4);
    Pixel &pixel = out[x];
    pixel.opaque = colour != 0;
    pixel.high = (entry & 0x8000U) != 0;
    pixel.index =
        static_cast<std::uint8_t>(((entry >> 13) & 3U) * 16U + colour);
  }
}

void Renderer::window_line(const Vdp &vdp, std::uint32_t line,
                           std::span<Pixel> a) const {
  const unsigned columns = static_cast<unsigned>(a.size()) / 8U;
  const bool h40 = columns == 40U;
  const unsigned h_bound = (vdp.reg(17) & 0x1FU) * 2U; // in cells
  const bool h_right = (vdp.reg(17) & 0x80U) != 0;
  const unsigned v_bound = (vdp.reg(18) & 0x1FU) * 8U; // in lines
  const bool v_below = (vdp.reg(18) & 0x80U) != 0;
  const bool v_covers = v_below ? line >= v_bound : line < v_bound;
  const std::uint32_t table = std::uint32_t{vdp.reg(3) & (h40 ? 0x3CU : 0x3EU)}
                              << 10;
  const unsigned stride = h40 ? 64U : 32U;
  const auto &vram = vdp.vram();
  for (unsigned x = 0; x < columns * 8U; ++x) {
    const unsigned column = x / 8U;
    const bool h_covers = h_right ? column >= h_bound : column < h_bound;
    if (!v_covers && !h_covers)
      continue;
    const std::uint32_t entry_at =
        (table + ((line / 8U) * stride + column) * 2U) & 0xFFFFU;
    const std::uint16_t entry = static_cast<std::uint16_t>(
        (std::uint32_t{vram[entry_at]} << 8) | vram[entry_at + 1U]);
    unsigned tile_x = x & 7U;
    const unsigned cell_h = double_res_ ? 16U : 8U;
    unsigned tile_y = (double_res_ ? line * 2U + field_ : line) % cell_h;
    if (entry & 0x0800U)
      tile_x = 7U - tile_x;
    if (entry & 0x1000U)
      tile_y = cell_h - 1U - tile_y;
    const std::uint32_t tile_at =
        ((std::uint32_t{entry & 0x7FFU} * cell_h * 4U) + tile_y * 4U +
         tile_x / 2U) &
        0xFFFFU;
    const std::uint8_t byte = vram[tile_at];
    const unsigned colour = (tile_x & 1U) ? (byte & 0x0FU) : (byte >> 4);
    Pixel &pixel = a[x];
    pixel.opaque = colour != 0;
    pixel.high = (entry & 0x8000U) != 0;
    pixel.index =
        static_cast<std::uint8_t>(((entry >> 13) & 3U) * 16U + colour);
  }
}

void Renderer::sprite_line(const Vdp &vdp, std::uint32_t line,
                           std::span<Pixel> out) {
  const bool h40 = out.size() == 320U;
  const unsigned max_frame = h40 ? 80U : 64U;
  const unsigned max_line = h40 ? 20U : 16U;
  const unsigned max_pixels = h40 ? 320U : 256U;
  const std::uint32_t table = std::uint32_t{vdp.reg(5) & (h40 ? 0x7CU : 0x7EU)}
                              << 9;
  const auto &vram = vdp.vram();
  const auto byte = [&](std::uint32_t at) { return vram[at & 0xFFFFU]; };
  // Measured on gpgx (sprite_limits 17 to 25, 101 to 176): pixels a sprite
  // leaves outside the screen are kept from line to line and collide with
  // later ones, from column width + 1 on. When the list holds one sprite
  // (the first link is 0) the column just past the edge and the whole left
  // margin are kept too.
  const bool lone_sprite = (byte(table + 3U) & 0x7FU) == 0;
  unsigned index = 0;
  unsigned on_line = 0;
  bool seen_x_nonzero = false;
  unsigned pixels = 0;
  for (unsigned walked = 0; walked < max_frame; ++walked) {
    const std::uint32_t at = table + index * 8U;
    const int y = static_cast<int>(((byte(at) << 8) | byte(at + 1U)) &
                                   (double_res_ ? 0x7FFU : 0x3FFU)) -
                  (double_res_ ? 256 : 128);
    const unsigned size = byte(at + 2U) & 0x0FU;
    const unsigned link = byte(at + 3U) & 0x7FU;
    const unsigned rows = (size & 3U) + 1U;
    const unsigned cols = ((size >> 2) & 3U) + 1U;
    const unsigned cell_h = double_res_ ? 16U : 8U;
    const int row =
        static_cast<int>(double_res_ ? line * 2U + field_ : line) - y;
    if (row >= 0 && row < static_cast<int>(rows * cell_h)) {
      if (on_line >= max_line) {
        flags_.overflow = true;
        break;
      }
      ++on_line;
      const std::uint16_t attr =
          static_cast<std::uint16_t>((byte(at + 4U) << 8) | byte(at + 5U));
      const int x =
          static_cast<int>(((byte(at + 6U) << 8) | byte(at + 7U)) & 0x1FFU) -
          128;
      // X = 0 masks the sprites after it once a sprite with another X has
      // been met on the line.
      if (x == -128) {
        if (seen_x_nonzero)
          break;
      } else {
        seen_x_nonzero = true;
      }
      const unsigned width = cols * 8U;
      unsigned drawn = std::min(width, max_pixels - pixels);
      pixels += drawn;
      // A sprite wholly outside the screen draws nothing, not even into the
      // columns kept outside it (measured: sprite_limits 101 to 111, 145).
      if (x + static_cast<int>(width) <= 0 || x >= static_cast<int>(out.size()))
        drawn = 0;
      unsigned sy = static_cast<unsigned>(row);
      if (attr & 0x1000U)
        sy = rows * cell_h - 1U - sy;
      for (unsigned sx = 0; sx < drawn; ++sx) {
        const int column = x + static_cast<int>(sx);
        if (column < -128 || column >= 384)
          continue;
        unsigned tx = sx;
        if (attr & 0x0800U)
          tx = width - 1U - tx;
        const std::uint32_t tile =
            (attr & 0x7FFU) + (tx / 8U) * rows + sy / cell_h;
        const std::uint8_t b =
            byte(tile * cell_h * 4U + (sy % cell_h) * 4U + (tx & 7U) / 2U);
        const unsigned colour = ((tx & 1U) ? b : (b >> 4)) & 0x0FU;
        if (colour == 0)
          continue;
        if (column < 0 || column >= static_cast<int>(out.size())) {
          const bool kept =
              lone_sprite || column > static_cast<int>(out.size());
          if (!kept)
            continue;
          bool &seen = offscreen_[static_cast<unsigned>(column + 128)];
          flags_.collision |= seen;
          seen = true;
          continue;
        }
        Pixel &pixel = out[static_cast<unsigned>(column)];
        if (pixel.opaque) {
          flags_.collision = true;
          continue;
        }
        pixel.opaque = true;
        pixel.high = (attr & 0x8000U) != 0;
        pixel.index =
            static_cast<std::uint8_t>(((attr >> 13) & 3U) * 16U + colour);
      }
    }
    if (link == 0 || link >= max_frame)
      break;
    index = link;
  }
}

void Renderer::render_line(const Vdp &vdp, std::uint32_t line) {
  if (line >= size_.height ||
      frame_.size() < std::size_t{size_.width} * size_.height)
    return;
  // Register 0 bit 2 clear (limited palette): gpgx keeps only the lowest bit
  // of each 3-bit channel (color_update_m5, data &= 0x49).
  const bool limited = (vdp.reg(0) & 0x04U) == 0;
  const auto &cram = vdp.cram();
  const auto cram_colour = [&](unsigned index, Shade shade) {
    std::uint16_t value = cram[index & 0x3FU];
    if (limited)
      value &= 0x0222U;
    return colour_to_rgba(value, shade);
  };
  const bool shadow_mode = (vdp.reg(12) & 0x08U) != 0;
  const unsigned width = size_.width;
  const auto row = std::span<std::uint32_t>(frame_).subspan(
      std::size_t{line} * width, width);
  const std::uint32_t backdrop_index = vdp.reg(7) & 0x3FU;
  const bool display_on = (vdp.reg(1) & 0x40U) != 0;
  if (!display_on) {
    std::fill(row.begin(), row.end(),
              cram_colour(backdrop_index, Shade::normal));
  } else {
    std::vector<Pixel> a(width), b(width), sp(width);
    sprite_line(vdp, line, sp);
    plane_line(vdp, 0, line, a);
    plane_line(vdp, 1, line, b);
    window_line(vdp, line, a);
    for (unsigned x = 0; x < width; ++x) {
      unsigned index = backdrop_index;
      const Pixel &pa = a[x];
      const Pixel &pb = b[x];
      // Shadow when neither plane has the priority bit set in its tile, even
      // where the pixel is transparent (measured on gpgx, modes_sweep 13).
      const bool high = pa.high || pb.high;
      Pixel ps = sp[x];
      Shade shade = shadow_mode && !high ? Shade::shadow : Shade::normal;
      // Shadow/highlight operators: palette 3 colours 14 and 15 of a sprite
      // are not drawn, they change the brightness of what lies below.
      if (shadow_mode && ps.opaque &&
          (ps.index == 0x3EU || ps.index == 0x3FU)) {
        // An operator under a high priority plane pixel has no effect
        // (measured on gpgx, sprite_limits 210).
        const bool covered =
            !ps.high && ((pa.opaque && pa.high) || (pb.opaque && pb.high));
        if (!covered) {
          if (ps.index == 0x3FU)
            shade = Shade::shadow;
          else
            shade = shade == Shade::shadow ? Shade::normal : Shade::highlight;
        }
        ps = Pixel{};
      } else if (shadow_mode && ps.opaque) {
        // gpgx quirk (sprite_limits 211): colour 14 of a low priority sprite
        // is not shadowed, in any palette.
        shade = high || ps.high || (ps.index & 15U) == 14U ? Shade::normal
                                                           : Shade::shadow;
      }
      if (ps.opaque && ps.high) {
        index = ps.index;
      } else if (pa.opaque && pa.high) {
        index = pa.index;
      } else if (pb.opaque && pb.high) {
        index = pb.index;
      } else if (ps.opaque) {
        index = ps.index;
      } else if (pa.opaque) {
        index = pa.index;
      } else if (pb.opaque) {
        index = pb.index;
      }
      row[x] = cram_colour(index, shade);
    }
    if ((vdp.reg(0) & 0x20U) != 0) // left column blank: backdrop
      for (unsigned x = 0; x < 8U && x < width; ++x)
        row[x] = cram_colour(backdrop_index,
                             shadow_mode ? Shade::shadow : Shade::normal);
  }
  if (hook_)
    hook_(line, row);
}

} // namespace tessera::md
