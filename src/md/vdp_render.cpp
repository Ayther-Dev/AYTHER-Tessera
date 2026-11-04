#include "md/vdp_render.hpp"

#include <algorithm>

namespace tessera::md {

FrameSize frame_size(const Vdp &vdp) noexcept {
  FrameSize size;
  size.width = (vdp.reg(12) & 0x81U) != 0 ? 320U : 256U;
  size.height = (vdp.reg(1) & 0x08U) != 0 ? 240U : 224U;
  return size;
}

std::uint32_t colour_to_rgba(std::uint16_t cram, Shade shade) noexcept {
  // Measured on gpgx (palette_sweep, all 512 colours): the 3 bit level is
  // widened to a 4 bit level L (normal 2*level, shadow level, highlight
  // level + 7), then to the 565 output by bit replication (5 bits
  // (L<<1)|(L>>3), 6 bits (L<<2)|(L>>2)), and a host expands that back to 8
  // bits.
  const auto expand = [shade](unsigned level, unsigned bits) {
    const unsigned l4 = shade == Shade::normal   ? level * 2U
                        : shade == Shade::shadow ? level
                                                 : level + 7U;
    return bits == 5 ? ((l4 << 1) | (l4 >> 3)) : ((l4 << 2) | (l4 >> 2));
  };
  const std::uint32_t r5 = expand((cram >> 1) & 7U, 5);
  const std::uint32_t g6 = expand((cram >> 5) & 7U, 6);
  const std::uint32_t b5 = expand((cram >> 9) & 7U, 5);
  const std::uint32_t red = (r5 << 3) | (r5 >> 2);
  const std::uint32_t green = (g6 << 2) | (g6 >> 4);
  const std::uint32_t blue = (b5 << 3) | (b5 >> 2);
  return red | (green << 8) | (blue << 16) | 0xFF000000U;
}

void Renderer::begin_frame(const Vdp &vdp) {
  size_ = frame_size(vdp);
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
  const unsigned pixels_y = cells_y * 8U;
  for (unsigned x = 0; x < columns * 8U; ++x) {
    // The 2-cell vertical scroll columns are 16 pixel bands whose phase
    // follows the horizontal scroll (measured on gpgx): band edges sit at
    // x = (hscroll & 15) + 16 n; left of the first edge a partial band.
    const unsigned phase = hscroll & 15U;
    unsigned vscroll = 0;
    if (!two_cell_v) {
      vscroll = vdp.vsram()[plane] & 0x3FFU;
    } else {
      // The partial band left of the first edge scrolls by 0 in H32 and by
      // the AND of the last pair in H40 (measured).
      const int pair = static_cast<int>((x + 16U - phase) / 16U) - 1;
      if (pair >= 0 && pair < 20)
        vscroll =
            vdp.vsram()[static_cast<unsigned>(pair) * 2U + plane] & 0x3FFU;
      else if (pair < 0 && columns == 40U)
        vscroll = (vdp.vsram()[38] & vdp.vsram()[39]) & 0x3FFU;
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
    const unsigned py = (line + vscroll) % pixels_y;
    const unsigned cell_x = px / 8U;
    const unsigned cell_y = py / 8U;
    const std::uint32_t entry_at =
        (table + (cell_y * cells_x + cell_x) * 2U) & 0xFFFFU;
    const std::uint16_t entry = static_cast<std::uint16_t>(
        (std::uint32_t{vram[entry_at]} << 8) | vram[entry_at + 1U]);
    unsigned tile_x = px & 7U;
    unsigned tile_y = py & 7U;
    if (entry & 0x0800U)
      tile_x = 7U - tile_x;
    if (entry & 0x1000U)
      tile_y = 7U - tile_y;
    const std::uint32_t tile_at =
        ((std::uint32_t{entry & 0x7FFU} * 32U) + tile_y * 4U + tile_x / 2U) &
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
    unsigned tile_y = line & 7U;
    if (entry & 0x0800U)
      tile_x = 7U - tile_x;
    if (entry & 0x1000U)
      tile_y = 7U - tile_y;
    const std::uint32_t tile_at =
        ((std::uint32_t{entry & 0x7FFU} * 32U) + tile_y * 4U + tile_x / 2U) &
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

void Renderer::render_line(const Vdp &vdp, std::uint32_t line) {
  if (line >= size_.height ||
      frame_.size() < std::size_t{size_.width} * size_.height)
    return;
  // Register 0 bit 2 clear (limited palette): gpgx keeps only the lowest bit
  // of each 3-bit channel (color_update_m5, data &= 0x49).
  const bool limited = (vdp.reg(0) & 0x04U) == 0;
  const auto &cram = vdp.cram();
  const auto cram_colour = [&](unsigned index, bool shadow) {
    std::uint16_t value = cram[index & 0x3FU];
    if (limited)
      value &= 0x0222U;
    return colour_to_rgba(value, shadow ? Shade::shadow : Shade::normal);
  };
  const bool shadow_mode = (vdp.reg(12) & 0x08U) != 0;
  const unsigned width = size_.width;
  const auto row = std::span<std::uint32_t>(frame_).subspan(
      std::size_t{line} * width, width);
  const std::uint32_t backdrop_index = vdp.reg(7) & 0x3FU;
  const bool display_on = (vdp.reg(1) & 0x40U) != 0;
  if (!display_on) {
    std::fill(row.begin(), row.end(), cram_colour(backdrop_index, false));
  } else {
    std::vector<Pixel> a(width), b(width);
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
      if (pa.opaque && pa.high) {
        index = pa.index;
      } else if (pb.opaque && pb.high) {
        index = pb.index;
      } else if (pa.opaque) {
        index = pa.index;
      } else if (pb.opaque) {
        index = pb.index;
      }
      row[x] = cram_colour(index, shadow_mode && !high);
    }
    if ((vdp.reg(0) & 0x20U) != 0) // left column blank: backdrop
      for (unsigned x = 0; x < 8U && x < width; ++x)
        row[x] = cram_colour(backdrop_index, shadow_mode);
  }
  if (hook_)
    hook_(line, row);
}

} // namespace tessera::md
