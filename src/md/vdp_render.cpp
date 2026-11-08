#include "md/vdp_render.hpp"

#include <algorithm>

namespace tessera::md {

FrameSize frame_size(const Vdp &vdp) noexcept {
  FrameSize size;
  if (vdp.mode4_active()) {
    size.width = 256U;
    size.height = 192U;
    return size;
  }
  size.width = (vdp.reg(12) & 0x81U) != 0 ? 320U : 256U;
  size.height = (vdp.reg(1) & 0x08U) != 0 ? 240U : 224U;
  return size;
}

unsigned mode4_raw(std::uint16_t cram) noexcept {
  // CRAM keeps the Mode 4 word the way gpgx packs it for Mode 5 reads (3 bits
  // per channel, the low 6 bits of the word in R and G): undo that.
  return ((cram >> 1) & 7U) | (((cram >> 5) & 7U) << 3);
}

std::uint16_t mode4_565(unsigned raw) noexcept {
  const unsigned r = raw & 3U, g = (raw >> 2) & 3U, b = (raw >> 4) & 3U;
  const auto five = [](unsigned c) { return (c << 3) | (c << 1) | (c >> 1); };
  const auto six = [](unsigned c) { return (c << 4) | (c << 2) | c; };
  return static_cast<std::uint16_t>((five(r) << 11) | (six(g) << 5) | five(b));
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
  if (vdp.mode4_active()) {
    // Mode 4 (measured on gpgx): the 32 CRAM colours, 2 bits per channel
    // widened by bit replication, repeat in the shadow and highlight tables;
    // the backdrop (entry 16 + reg 7 & 15) sits at 64, 96, 192 and 224.
    std::array<std::uint16_t, 32> pal{};
    for (unsigned i = 0; i < 32U; ++i)
      pal[i] = mode4_565(mode4_raw(vdp.cram()[i]));
    for (unsigned i = 0; i < 64U; ++i) {
      table[i] = pal[i & 31U];
      table[128U + i] = pal[i & 31U];
    }
    const std::uint16_t back = pal[16U + (vdp.reg(7) & 15U)];
    table[64] = table[96] = table[192] = table[224] = back;
    return table;
  }
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
      value = vdp.transparent_cram()[i / 16U - 1U];
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
  // The width is the one at line 0, the height the one fixed at the start of
  // the frame (the vertical blanking): V30 set in the vertical interrupt
  // leaves a 224-line picture (measured, raster_bands 16 and 18).
  if (!vdp.mode4_active())
    size_.height = vdp.frame_active_lines();
  field_ ^= 1U;
  double_res_ = (vdp.reg(12) & 0x06U) == 0x06U;
  frame_.assign(std::size_t{size_.width} * size_.height, 0xFF000000U);
  // The entries beyond the count keep what earlier frames left there, as in
  // the reference (measured: Golden Axe 600); only the count and the outcome
  // start over.
  capture_.count = 0;
  capture_.outcome = {};
  captured_ = {};
}

// Measured on gpgx r4 (sprite_limits, 109 scenes, regions 13, 14 and 25): the
// parse phase lists, per line, the sprites of the chain that cross it, up to
// the line limit; the pixel budget cuts the list; X = 0 masks what follows a
// sprite with another X. A sprite is captured on the first line it enters a
// list, even if the budget or the mask then keep it from drawing.
void Renderer::capture_line(const Vdp &vdp, std::uint32_t line) {
  const bool h40 = size_.width == 320U;
  const unsigned max_frame = h40 ? 80U : 64U;
  const unsigned max_line = h40 ? 20U : 16U;
  const unsigned width_px = size_.width;
  const std::uint32_t table = std::uint32_t{vdp.reg(5) & (h40 ? 0x7EU : 0x7FU)}
                              << 9;
  const auto &vram = vdp.vram();
  const auto byte = [&](std::uint32_t at) { return vram[at & 0xFFFFU]; };
  const unsigned cell_h = double_res_ ? 16U : 8U;
  const int scan = static_cast<int>(double_res_ ? line * 2U + field_ : line);
  struct Listed {
    unsigned index;
    int x;
    unsigned columns;
  };
  std::array<Listed, 20> list{};
  unsigned listed = 0;
  unsigned pixels = 0;
  unsigned index = 0;
  for (unsigned walked = 0; walked < max_frame; ++walked) {
    const std::uint32_t at = table + index * 8U;
    const unsigned yr =
        ((byte(at) << 8) | byte(at + 1U)) & (double_res_ ? 0x7FFU : 0x3FFU);
    const unsigned size = byte(at + 2U) & 0x0FU;
    const unsigned link = byte(at + 3U) & 0x7FU;
    const unsigned rows = (size & 3U) + 1U;
    const unsigned columns = ((size >> 2) & 3U) + 1U;
    const int row = scan - (static_cast<int>(yr) - (double_res_ ? 256 : 128));
    if (row >= 0 && row < static_cast<int>(rows * cell_h)) {
      std::uint8_t &bits = capture_.outcome[index];
      if (listed >= max_line) {
        bits |= kOutcomeLineFull;
      } else {
        const unsigned xr = ((byte(at + 6U) << 8) | byte(at + 7U)) & 0x1FFU;
        if (!captured_[index]) {
          captured_[index] = true;
          capture_.sprites[capture_.count++] = {
              static_cast<std::uint16_t>(yr),
              static_cast<std::uint16_t>(xr),
              static_cast<std::uint16_t>((byte(at + 4U) << 8) | byte(at + 5U)),
              static_cast<std::uint8_t>(columns),
              static_cast<std::uint8_t>(rows),
              static_cast<std::uint8_t>(index),
              static_cast<std::uint8_t>(walked)};
        }
        if (pixels >= width_px) {
          bits |= kOutcomePixelFull;
        } else {
          bits |= kOutcomeParsed;
          list[listed++] = {index, static_cast<int>(xr) - 128, columns};
          pixels += columns * 8U;
          if (pixels >= width_px)
            bits |= kOutcomePixelFull;
        }
      }
    }
    if (link == 0 || link >= max_frame)
      break;
    index = link;
  }
  // Draw phase over the list: X = 0 masks the sprites after one with another X.
  bool seen_x = false;
  bool masked = false;
  unsigned used = 0;
  for (unsigned i = 0; i < listed; ++i) {
    const Listed &entry = list[i];
    std::uint8_t &bits = capture_.outcome[entry.index];
    if (entry.x == -128) {
      masked = masked || seen_x;
    } else {
      seen_x = true;
    }
    if (masked) {
      bits |= kOutcomeMasked;
      continue;
    }
    const unsigned width = entry.columns * 8U;
    const unsigned drawn =
        std::min(width, width_px > used ? width_px - used : 0U);
    used += width;
    if (drawn > 0 && entry.x + static_cast<int>(width) > 0 &&
        entry.x < static_cast<int>(width_px))
      bits |= kOutcomeDrawn;
  }
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
  const std::uint8_t size_reg = start_reg(vdp, line, 16);
  const unsigned cells_x = (size_reg & 3U) == 0   ? 32U
                           : (size_reg & 3U) == 1 ? 64U
                                                  : 128U;
  const unsigned cells_y = ((size_reg >> 4) & 3U) == 0   ? 32U
                           : ((size_reg >> 4) & 3U) == 1 ? 64U
                                                         : 128U;
  const std::uint32_t table = plane == 0
                                  ? (std::uint32_t{vdp.reg(2) & 0x38U} << 10)
                                  : (std::uint32_t{vdp.reg(4) & 0x07U} << 13);
  // The VRAM at the line start when the line was latched (System), else the
  // live one (a renderer driven alone, as in the unit tests).
  const auto &vram =
      latch_.line == line ? vdp.vram_at_line_start() : vdp.vram();
  const bool latched = latch_.line == line;
  const unsigned hscroll =
      latched ? latch_.hscroll[plane] : hscroll_value(vdp, plane, line);
  const auto &vsram = latched ? latch_.vsram : vdp.vsram();
  const bool two_cell_v = (start_reg(vdp, line, 11) & 0x04U) != 0;
  const std::uint8_t reg17 = start_reg(vdp, line, 17);
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
      vscroll = vsram[plane] & vmask;
    } else {
      // The partial band left of the first edge scrolls by 0 in H32 and by
      // the AND of the last pair in H40 (measured).
      const int pair = static_cast<int>((x + 16U - phase) / 16U) - 1;
      if (pair >= 0 && pair < 20)
        vscroll = vsram[static_cast<unsigned>(pair) * 2U + plane] & vmask;
      else if (pair < 0 && columns == 40U)
        vscroll = (vsram[38] & vsram[39]) & vmask;
    }
    // Left window: for the first (hscroll & 15) pixels after the boundary
    // plane A shows the tiles two columns on (measured on gpgx).
    unsigned skew = 0;
    if (plane == 0 && (reg17 & 0x80U) == 0 && (reg17 & 0x1FU) != 0) {
      const unsigned edge = (reg17 & 0x1FU) * 16U;
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
  const std::uint8_t reg17 = start_reg(vdp, line, 17);
  const std::uint8_t reg18 = start_reg(vdp, line, 18);
  const unsigned h_bound = (reg17 & 0x1FU) * 2U; // in cells
  const bool h_right = (reg17 & 0x80U) != 0;
  const unsigned v_bound = (reg18 & 0x1FU) * 8U; // in lines
  const bool v_below = (reg18 & 0x80U) != 0;
  const bool v_covers = v_below ? line >= v_bound : line < v_bound;
  const std::uint32_t table = std::uint32_t{vdp.reg(3) & (h40 ? 0x3CU : 0x3EU)}
                              << 10;
  const unsigned stride = h40 ? 64U : 32U;
  // The VRAM at the line start when the line was latched (System), else the
  // live one (a renderer driven alone, as in the unit tests).
  const auto &vram =
      latch_.line == line ? vdp.vram_at_line_start() : vdp.vram();
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
  const std::uint32_t table = std::uint32_t{vdp.reg(5) & (h40 ? 0x7EU : 0x7FU)}
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

void Renderer::render_line_m4(const Vdp &vdp, std::uint32_t line) {
  const auto &vram = vdp.vram();
  const auto rgba = [&](unsigned index) {
    const std::uint32_t c = mode4_565(mode4_raw(vdp.cram()[index & 31U]));
    const std::uint32_t r5 = c >> 11, g6 = (c >> 5) & 63U, b5 = c & 31U;
    return ((r5 << 3) | (r5 >> 2)) | (((g6 << 2) | (g6 >> 4)) << 8) |
           (((b5 << 3) | (b5 >> 2)) << 16) | 0xFF000000U;
  };
  const unsigned width = size_.width;
  const auto row = std::span<std::uint32_t>(frame_).subspan(
      std::size_t{line} * width, width);
  const std::uint32_t backdrop = rgba(16U + (vdp.reg(7) & 15U));
  if ((vdp.reg(1) & 0x40U) == 0) {
    std::fill(row.begin(), row.end(), backdrop);
    return;
  }
  // background: 32 x 28 name table entries (2 bytes, low byte first)
  struct Bg {
    unsigned colour = 0;
    unsigned index = 0;
    bool priority = false;
  };
  std::vector<Bg> bg(width);
  const std::uint32_t table = std::uint32_t{vdp.reg(2) & 0x0EU} << 10;
  const bool lock_top = (vdp.reg(0) & 0x40U) != 0 && line < 16U;
  const unsigned hscroll = lock_top ? 0U : vdp.reg(8);
  for (unsigned x = 0; x < width; ++x) {
    // Measured on gpgx (mode4_scene 13, 16-18): register 9 has no effect on
    // the picture, whatever its value or the moment it is written.
    const unsigned vscroll = 0U;
    const unsigned sx = (x + 256U - hscroll) & 255U;
    const unsigned sy = (line + vscroll) % 224U;
    const std::uint32_t at = table + ((sy / 8U) * 32U + sx / 8U) * 2U;
    const unsigned entry =
        vram[at & 0xFFFFU] | (vram[(at + 1U) & 0xFFFFU] << 8);
    unsigned tx = sx & 7U, ty = sy & 7U;
    if (entry & 0x0200U)
      tx = 7U - tx;
    if (entry & 0x0400U)
      ty = 7U - ty;
    const std::uint32_t tile = ((entry & 0x1FFU) * 32U + ty * 4U) & 0xFFFFU;
    unsigned colour = 0;
    for (unsigned plane = 0; plane < 4U; ++plane)
      colour |= ((vram[(tile + plane) & 0xFFFFU] >> (7U - tx)) & 1U) << plane;
    bg[x].colour = colour;
    bg[x].index = colour + ((entry & 0x0800U) ? 16U : 0U);
    bg[x].priority = (entry & 0x1000U) != 0;
  }
  // sprites: up to 8 per line, the first of the list on top
  std::vector<int> sprite(width, -1);
  const std::uint32_t sat = std::uint32_t{vdp.reg(5) & 0x7EU} << 7;
  const bool tall = (vdp.reg(1) & 0x02U) != 0;
  const unsigned height = tall ? 16U : 8U;
  unsigned shown = 0;
  for (unsigned n = 0; n < 64U; ++n) {
    const unsigned y = vram[(sat + n) & 0xFFFFU];
    if (y == 0xD0U)
      break;
    const int top = static_cast<int>((y + 1U) & 0xFFU);
    const int top_s = top > 0xF0 ? top - 256 : top;
    const int r = static_cast<int>(line) - top_s;
    if (r < 0 || r >= static_cast<int>(height))
      continue;
    if (shown == 8U) {
      flags_.overflow = true;
      break;
    }
    ++shown;
    const int sx0 = vram[(sat + 128U + n * 2U) & 0xFFFFU] -
                    ((vdp.reg(0) & 0x08U) != 0 ? 8 : 0);
    unsigned tile = vram[(sat + 129U + n * 2U) & 0xFFFFU];
    if (tall)
      tile &= 0xFEU;
    tile += (vdp.reg(6) & 0x04U) != 0 ? 256U : 0U;
    const std::uint32_t at =
        ((tile * 32U) + static_cast<unsigned>(r) * 4U) & 0xFFFFU;
    for (unsigned px = 0; px < 8U; ++px) {
      const int x = sx0 + static_cast<int>(px);
      if (x < 0 || x >= static_cast<int>(width))
        continue;
      unsigned colour = 0;
      for (unsigned plane = 0; plane < 4U; ++plane)
        colour |= ((vram[(at + plane) & 0xFFFFU] >> (7U - px)) & 1U) << plane;
      if (colour == 0)
        continue;
      if (sprite[static_cast<unsigned>(x)] >= 0)
        flags_.collision = true;
      else
        sprite[static_cast<unsigned>(x)] = static_cast<int>(16U + colour);
    }
  }
  for (unsigned x = 0; x < width; ++x) {
    // colour 0 of a tile shows colour 0 of its palette (not the backdrop)
    unsigned index = bg[x].index;
    const bool show_bg = bg[x].colour != 0;
    if (sprite[x] >= 0 && !(show_bg && bg[x].priority))
      index = static_cast<unsigned>(sprite[x]);
    row[x] = rgba(index);
  }
  if ((vdp.reg(0) & 0x20U) != 0)
    for (unsigned x = 0; x < 8U; ++x)
      row[x] = backdrop;
  if (hook_)
    hook_(line, row);
}

void Renderer::latch_line(const Vdp &vdp, std::uint32_t line) {
  latch_.line = line;
  latch_.hscroll = {hscroll_value(vdp, 0, line), hscroll_value(vdp, 1, line)};
  latch_.vsram = vdp.vsram();
  latch_.shadow = (vdp.reg(12) & 0x08U) != 0;
  for (unsigned i = 0; i < latch_.regs.size(); ++i)
    latch_.regs[i] = vdp.reg(i);
  latch_.cram = vdp.cram();
  latch_.window_clip = vdp.window_clip();
}

void Renderer::capture_line_state(const Vdp &vdp, std::uint32_t line) {
  if (line >= line_regs_.size() || latch_.line != line)
    return;
  const auto &r = latch_.regs;
  // A line is captured when the display is on at its start or when it is
  // drawn: a write in its horizontal interrupt that turns the display off
  // (line 7) or on (line 15) still leaves an entry, with register 1 as it
  // was while on (measured, raster_bands 5). Otherwise the entry keeps what
  // it had.
  const bool on_at_start = (r[1] & 0x40U) != 0;
  if (!on_at_start && (vdp.reg(1) & 0x40U) == 0)
    return;
  const bool h40 = (r[12] & 0x81U) != 0;
  // Plane A clip in 2-cell units from register 17 at the line start
  // (raster_bands 30, 31).
  const std::uint8_t width = h40 ? 20U : 16U;
  const std::uint8_t split =
      static_cast<std::uint8_t>(std::min<unsigned>(r[17] & 0x1FU, width));
  std::uint8_t a_start = 0, a_end = width, flags = 0;
  if ((r[17] & 0x1FU) != 0) {
    flags = 1;
    if ((r[17] & 0x80U) != 0) // window on the right
      a_end = split;
    else
      a_start = split;
  }
  const auto clip_w = latch_.window_clip;
  LineRegs &e = line_regs_[line];
  const auto put16 = [&](std::size_t at, std::uint32_t value) {
    e[at] = static_cast<std::uint8_t>(value);
    e[at + 1] = static_cast<std::uint8_t>(value >> 8);
  };
  put16(0, latch_.hscroll[0]);
  put16(2, latch_.hscroll[1]);
  put16(4, latch_.vsram[0]);
  put16(6, latch_.vsram[1]);
  put16(8, std::uint32_t{vdp.reg(2) & 0x38U} << 10);
  put16(10, std::uint32_t{vdp.reg(4) & 0x07U} << 13);
  put16(12, std::uint32_t{vdp.reg(3) & (h40 ? 0x3CU : 0x3EU)} << 10);
  put16(14, std::uint32_t{r[13] & 0x3FU} << 10);
  put16(16, std::uint32_t{r[5] & (h40 ? 0x7EU : 0x7FU)} << 9);
  e[18] = on_at_start ? r[1] : vdp.reg(1);
  e[19] = r[7];
  e[20] = r[11];
  e[21] = r[12];
  e[22] = r[13];
  e[23] = r[16];
  e[24] = r[17];
  e[25] = r[18];
  e[26] = a_start;
  e[27] = a_end;
  e[28] = clip_w[0];
  e[29] = clip_w[1];
  e[30] = flags;
  e[31] = 0;
  for (unsigned i = 0; i < 64U; ++i) {
    const std::uint16_t c = latch_.cram[i];
    line_cram_[line][i] = static_cast<std::uint16_t>(
        ((c >> 9 & 7U) << 6) | ((c >> 5 & 7U) << 3) | (c >> 1 & 7U));
  }
}

std::uint8_t Renderer::start_reg(const Vdp &vdp, std::uint32_t line,
                                 unsigned index) const noexcept {
  return latch_.line == line ? latch_.regs[index & 31U] : vdp.reg(index);
}

void Renderer::render_line(const Vdp &vdp, std::uint32_t line) {
  if (line >= size_.height ||
      frame_.size() < std::size_t{size_.width} * size_.height)
    return;
  if (vdp.mode4_active()) {
    render_line_m4(vdp, line);
    return;
  }
  capture_line_state(vdp, line);
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
  const bool shadow_mode =
      latch_.line == line ? latch_.shadow : (vdp.reg(12) & 0x08U) != 0;
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
    if (capture_on_)
      capture_line(vdp, line);
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

void Renderer::save(StateWriter &w) const {
  w.boolean(flags_.overflow);
  w.boolean(flags_.collision);
  w.u32(field_);
  w.boolean(double_res_);
  for (const bool b : offscreen_)
    w.boolean(b);
}

void Renderer::load(StateReader &r) {
  flags_.overflow = r.boolean();
  flags_.collision = r.boolean();
  field_ = r.u32() & 1U;
  double_res_ = r.boolean();
  for (auto &b : offscreen_)
    b = r.boolean();
}

} // namespace tessera::md
