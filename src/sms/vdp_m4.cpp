#include "sms/vdp_m4.hpp"

#include <algorithm>

namespace tessera::sms {
namespace {

// Standard TMS9918A 16-color palette (0xFFRRGGBB)
constexpr std::uint32_t kTmsPalette[16] = {
    0xFF000000U, 0xFF000000U, 0xFF21C842U, 0xFF5EDC78U,
    0xFF5455EDU, 0xFF7D76FCU, 0xFFD4524DU, 0xFF42EBF5U,
    0xFFFC5554U, 0xFFFF7978U, 0xFFD4C154U, 0xFFE6CE80U,
    0xFF21B03BU, 0xFFC95BBAU, 0xFFCCCCCCU, 0xFFFFFFFFU};

inline std::uint8_t expand_2bit(std::uint8_t v) noexcept {
  return static_cast<std::uint8_t>((v & 3U) * 85U);
}

inline std::uint8_t expand_4bit(std::uint8_t v) noexcept {
  return static_cast<std::uint8_t>(((v & 0x0FU) << 4) | (v & 0x0FU));
}

} // namespace

VdpM4::VdpM4(ConsoleModel model) noexcept : model_(model) { reset(); }

void VdpM4::reset() noexcept {
  vram_.fill(0);
  cram_.fill(0);
  regs_.fill(0);
  regs_[2] = 0xFF;
  regs_[3] = 0xFF;
  regs_[4] = 0xFF;
  regs_[5] = 0xFF;
  regs_[10] = 0xFF;
  fb_.fill(0xFF000000U);
  addr_ = 0;
  code_ = 0;
  latch_ = 0;
  control_pending_ = false;
  read_buffer_ = 0;
  status_ = 0;
  gg_cram_latch_ = 0;
  line_counter_ = 0xFF;
  vscroll_latch_ = 0;
  v_counter_ = 0;
  h_counter_ = 0;
  vint_pending_ = false;
  hint_pending_ = false;
}

std::uint8_t VdpM4::read_data() noexcept {
  control_pending_ = false;
  const std::uint8_t ret = read_buffer_;
  read_buffer_ = vram_[addr_ & 0x3FFFU];
  addr_ = static_cast<std::uint16_t>((addr_ + 1U) & 0x3FFFU);
  return ret;
}

std::uint8_t VdpM4::read_control() noexcept {
  control_pending_ = false;
  const std::uint8_t s = status_;
  status_ &= ~0xE0U;
  vint_pending_ = false;
  hint_pending_ = false;
  return s;
}

void VdpM4::write_data(std::uint8_t value) noexcept {
  control_pending_ = false;
  read_buffer_ = value;
  if (code_ == 3U && model_ != ConsoleModel::sg1000) {
    if (model_ == ConsoleModel::game_gear) {
      if ((addr_ & 1U) == 0U) {
        gg_cram_latch_ = value;
      } else {
        const std::uint8_t idx = static_cast<std::uint8_t>(addr_ & 0x3EU);
        cram_[idx] = gg_cram_latch_;
        cram_[idx + 1U] = value;
      }
    } else {
      cram_[addr_ & 0x1FU] = value;
    }
  } else {
    vram_[addr_ & 0x3FFFU] = value;
  }
  addr_ = static_cast<std::uint16_t>((addr_ + 1U) & 0x3FFFU);
}

void VdpM4::write_control(std::uint8_t value) noexcept {
  if (!control_pending_) {
    latch_ = value;
    control_pending_ = true;
    return;
  }
  control_pending_ = false;
  addr_ = static_cast<std::uint16_t>(
      ((static_cast<std::uint16_t>(value) & 0x3FU) << 8) | latch_);
  code_ = static_cast<std::uint8_t>((value >> 6) & 0x03U);
  if (code_ == 0U) {
    read_buffer_ = vram_[addr_ & 0x3FFFU];
    addr_ = static_cast<std::uint16_t>((addr_ + 1U) & 0x3FFFU);
  } else if (code_ == 2U) {
    const std::uint8_t reg = static_cast<std::uint8_t>(value & 0x0FU);
    regs_[reg] = latch_;
  }
}

void VdpM4::start_frame() noexcept {
  vscroll_latch_ = regs_[9];
  line_counter_ = regs_[10];
}

void VdpM4::end_active_display() noexcept {
  status_ |= 0x80U;
  if ((regs_[1] & 0x20U) != 0)
    vint_pending_ = true;
}

std::uint32_t VdpM4::sms_palette_argb(std::uint8_t index) const noexcept {
  const std::uint8_t c = cram_[index & 0x1FU];
  const std::uint8_t r = expand_2bit(c & 0x03U);
  const std::uint8_t g = expand_2bit((c >> 2) & 0x03U);
  const std::uint8_t b = expand_2bit((c >> 4) & 0x03U);
  return 0xFF000000U | (std::uint32_t{r} << 16) | (std::uint32_t{g} << 8) | b;
}

std::uint32_t VdpM4::gg_palette_argb(std::uint8_t index) const noexcept {
  const std::size_t off = std::size_t{index & 0x1FU} * 2U;
  const std::uint16_t c = static_cast<std::uint16_t>(
      cram_[off] | (std::uint16_t{cram_[off + 1U]} << 8));
  const std::uint8_t r = expand_4bit(static_cast<std::uint8_t>(c & 0x0FU));
  const std::uint8_t g =
      expand_4bit(static_cast<std::uint8_t>((c >> 4) & 0x0FU));
  const std::uint8_t b =
      expand_4bit(static_cast<std::uint8_t>((c >> 8) & 0x0FU));
  return 0xFF000000U | (std::uint32_t{r} << 16) | (std::uint32_t{g} << 8) | b;
}

void VdpM4::render_line(int line) noexcept {
  if (line < 0 || line >= static_cast<int>(kSmsHeight))
    return;

  if (line == 0)
    start_frame();

  if (line_counter_ == 0) {
    line_counter_ = regs_[10];
    if ((regs_[0] & 0x10U) != 0)
      hint_pending_ = true;
  } else {
    --line_counter_;
  }

  const bool display_on = (regs_[1] & 0x40U) != 0;
  const bool mode4 =
      (model_ != ConsoleModel::sg1000) && ((regs_[0] & 0x04U) != 0);

  if (!display_on) {
    const std::uint32_t backdrop =
        mode4 ? (model_ == ConsoleModel::game_gear
                     ? gg_palette_argb(16U + (regs_[7] & 0x0FU))
                     : sms_palette_argb(16U + (regs_[7] & 0x0FU)))
              : kTmsPalette[regs_[7] & 0x0FU];
    auto *row =
        fb_.data() + std::size_t{static_cast<std::size_t>(line)} * kSmsWidth;
    std::fill(row, row + kSmsWidth, backdrop);
    return;
  }

  if (mode4)
    render_mode4_line(line);
  else
    render_tms_line(line);
}

void VdpM4::render_mode4_line(int line) noexcept {
  auto color_at = [&](std::uint8_t idx) -> std::uint32_t {
    return model_ == ConsoleModel::game_gear ? gg_palette_argb(idx)
                                             : sms_palette_argb(idx);
  };

  const std::uint32_t backdrop = color_at(16U + (regs_[7] & 0x0FU));
  auto *row =
      fb_.data() + std::size_t{static_cast<std::size_t>(line)} * kSmsWidth;

  const std::uint16_t name_base =
      static_cast<std::uint16_t>((regs_[2] & 0x0EU) << 10);
  const bool lock_top = (regs_[0] & 0x40U) != 0;
  const bool lock_right = (regs_[0] & 0x80U) != 0;
  const bool mask_left = (regs_[0] & 0x20U) != 0;
  const std::uint8_t hscroll = (lock_top && line < 16) ? 0U : regs_[8];

  std::array<std::uint8_t, kSmsWidth> bg_prio{};
  std::array<std::uint8_t, kSmsWidth> bg_opaque{};

  for (int x = 0; x < 256; ++x) {
    if (mask_left && x < 8) {
      row[x] = backdrop;
      continue;
    }
    const int col = x >> 3;
    const int scrolled_x = (x - hscroll) & 0xFF;
    const int eff_y =
        (lock_right && col >= 24) ? line : ((line + vscroll_latch_) % 224);

    const std::uint16_t map_addr = static_cast<std::uint16_t>(
        name_base + ((eff_y >> 3) * 32 + (scrolled_x >> 3)) * 2U);
    const std::uint16_t entry = static_cast<std::uint16_t>(
        vram_[map_addr & 0x3FFFU] |
        (std::uint16_t{vram_[(map_addr + 1U) & 0x3FFFU]} << 8));

    const std::uint16_t tile = entry & 0x01FFU;
    const bool hflip = (entry & 0x0200U) != 0;
    const bool vflip = (entry & 0x0400U) != 0;
    const std::uint8_t pal_off = (entry & 0x0800U) != 0 ? 16U : 0U;
    const bool prio = (entry & 0x1000U) != 0;

    const int ty = vflip ? (7 - (eff_y & 7)) : (eff_y & 7);
    const int tx = hflip ? (scrolled_x & 7) : (7 - (scrolled_x & 7));
    const std::uint16_t pat_addr =
        static_cast<std::uint16_t>(tile * 32U + ty * 4U);

    std::uint8_t cidx = 0;
    for (int bp = 0; bp < 4; ++bp) {
      cidx |= static_cast<std::uint8_t>(
          ((vram_[(pat_addr + bp) & 0x3FFFU] >> tx) & 1U) << bp);
    }

    row[x] = color_at(pal_off + cidx);
    bg_opaque[x] = cidx != 0 ? 1U : 0U;
    bg_prio[x] = (prio && cidx != 0) ? 1U : 0U;
  }

  // Sprites (64 entries in SAT)
  const std::uint16_t sat_base =
      static_cast<std::uint16_t>((regs_[5] & 0x7EU) << 7);
  const std::uint16_t spr_pat_base =
      (regs_[6] & 0x04U) != 0 ? 0x2000U : 0x0000U;
  const int spr_h = (regs_[1] & 0x02U) != 0 ? 16 : 8;
  const int shift_x = (regs_[0] & 0x08U) != 0 ? -8 : 0;

  std::array<bool, kSmsWidth> spr_drawn{};
  int count = 0;
  for (int i = 0; i < 64; ++i) {
    int sy = vram_[(sat_base + i) & 0x3FFFU];
    if (sy == 0xD0)
      break;
    if (sy >= 0xF0)
      sy -= 256;
    sy += 1;
    if (line < sy || line >= sy + spr_h)
      continue;
    if (++count > 8) {
      status_ |= 0x40U; // sprite overflow
      break;
    }

    const int sx =
        static_cast<int>(vram_[(sat_base + 128 + i * 2) & 0x3FFFU]) + shift_x;
    std::uint8_t tile = vram_[(sat_base + 128 + i * 2 + 1) & 0x3FFFU];
    if (spr_h == 16)
      tile &= 0xFEU;

    const int ty = line - sy;
    const std::uint16_t pat =
        static_cast<std::uint16_t>(spr_pat_base + tile * 32U + ty * 4U);
    for (int px = 0; px < 8; ++px) {
      const int x = sx + px;
      if (x < 0 || x >= 256 || (mask_left && x < 8))
        continue;
      const int bit = 7 - px;
      std::uint8_t cidx = 0;
      for (int bp = 0; bp < 4; ++bp) {
        cidx |= static_cast<std::uint8_t>(
            ((vram_[(pat + bp) & 0x3FFFU] >> bit) & 1U) << bp);
      }
      if (cidx == 0)
        continue;
      if (spr_drawn[x]) {
        status_ |= 0x20U; // sprite collision
        continue;
      }
      spr_drawn[x] = true;
      if (!bg_prio[x])
        row[x] = color_at(16U + cidx);
    }
  }
}

void VdpM4::render_tms_line(int line) noexcept {
  const std::uint8_t mode = static_cast<std::uint8_t>(
      ((regs_[0] & 0x02U) << 1) | ((regs_[1] & 0x18U) >> 3));
  const std::uint32_t backdrop = kTmsPalette[regs_[7] & 0x0FU];
  const std::uint32_t fg_default = kTmsPalette[(regs_[7] >> 4) & 0x0FU];
  auto *row =
      fb_.data() + std::size_t{static_cast<std::size_t>(line)} * kSmsWidth;

  const std::uint16_t name_base = static_cast<std::uint16_t>(regs_[2] & 0x0FU)
                                  << 10;
  const std::uint16_t color_base =
      (mode & 0x04U) != 0 ? static_cast<std::uint16_t>(regs_[3] & 0x80U) << 6
                          : static_cast<std::uint16_t>(regs_[3]) << 6;
  const std::uint16_t pat_base =
      (mode & 0x04U) != 0 ? static_cast<std::uint16_t>(regs_[4] & 0x04U) << 11
                          : static_cast<std::uint16_t>(regs_[4] & 0x07U) << 11;

  for (int x = 0; x < 256; ++x) {
    const int col = x >> 3;
    const int tile_row = line >> 3;
    const std::uint8_t tile =
        vram_[(name_base + tile_row * 32 + col) & 0x3FFFU];

    std::uint8_t fg = static_cast<std::uint8_t>((regs_[7] >> 4) & 0x0FU);
    std::uint8_t bg = static_cast<std::uint8_t>(regs_[7] & 0x0FU);
    bool bit = false;

    if ((mode & 0x04U) != 0) {
      // Mode 2 (Graphics II)
      const std::uint16_t bank =
          static_cast<std::uint16_t>((tile_row / 8) * 0x800U);
      const std::uint16_t pat_off = static_cast<std::uint16_t>(
          (pat_base + bank + tile * 8U + (line & 7)) & 0x3FFFU);
      const std::uint16_t col_off = static_cast<std::uint16_t>(
          (color_base + bank + tile * 8U + (line & 7)) & 0x3FFFU);
      const std::uint8_t pb = vram_[pat_off];
      const std::uint8_t cb = vram_[col_off];
      bit = ((pb >> (7 - (x & 7))) & 1U) != 0;
      fg = static_cast<std::uint8_t>(cb >> 4);
      bg = static_cast<std::uint8_t>(cb & 0x0FU);
    } else {
      // Mode 0 (Graphics I)
      const std::uint8_t pb =
          vram_[(pat_base + tile * 8U + (line & 7)) & 0x3FFFU];
      const std::uint8_t cb = vram_[(color_base + (tile >> 3)) & 0x3FFFU];
      bit = ((pb >> (7 - (x & 7))) & 1U) != 0;
      fg = static_cast<std::uint8_t>(cb >> 4);
      bg = static_cast<std::uint8_t>(cb & 0x0FU);
    }

    const std::uint8_t ci = bit ? fg : bg;
    row[x] = ci == 0 ? backdrop : (ci < 16 ? kTmsPalette[ci] : fg_default);
  }

  // TMS9918A sprites (max 4 per line)
  const std::uint16_t sat_base = static_cast<std::uint16_t>(regs_[5] & 0x7FU)
                                 << 7;
  const std::uint16_t spr_pat_base =
      static_cast<std::uint16_t>(regs_[6] & 0x07U) << 11;
  const bool size16 = (regs_[1] & 0x02U) != 0;
  const int spr_dim = size16 ? 16 : 8;
  std::array<bool, kSmsWidth> spr_drawn{};
  int count = 0;

  for (int i = 0; i < 32; ++i) {
    const std::uint16_t e = static_cast<std::uint16_t>(sat_base + i * 4);
    int sy = vram_[e & 0x3FFFU];
    if (sy == 0xD0)
      break;
    if (sy >= 0xE0)
      sy -= 256;
    sy += 1;
    if (line < sy || line >= sy + spr_dim)
      continue;
    if (++count > 4) {
      status_ =
          static_cast<std::uint8_t>((status_ & ~0x1FU) | 0x40U | (i & 0x1FU));
      break;
    }

    int sx = vram_[(e + 1U) & 0x3FFFU];
    std::uint8_t tile = vram_[(e + 2U) & 0x3FFFU];
    const std::uint8_t attr = vram_[(e + 3U) & 0x3FFFU];
    if (attr & 0x80U)
      sx -= 32;
    const std::uint8_t color = static_cast<std::uint8_t>(attr & 0x0FU);
    if (color == 0)
      continue;
    if (size16)
      tile &= 0xFCU;

    const int ty = line - sy;
    for (int px = 0; px < spr_dim; ++px) {
      const int x = sx + px;
      if (x < 0 || x >= 256)
        continue;
      const std::uint16_t p_addr = static_cast<std::uint16_t>(
          spr_pat_base + tile * 8U + (px >= 8 ? 16 : 0) + ty);
      const std::uint8_t p = vram_[p_addr & 0x3FFFU];
      if (((p >> (7 - (px & 7))) & 1U) == 0)
        continue;
      if (spr_drawn[x]) {
        status_ |= 0x20U;
        continue;
      }
      spr_drawn[x] = true;
      row[x] = kTmsPalette[color];
    }
  }
}

void VdpM4::save(md::StateWriter &w) const {
  w.bytes(vram_);
  w.bytes(cram_);
  w.bytes(regs_);
  w.u16(addr_);
  w.u8(code_);
  w.u8(latch_);
  w.boolean(control_pending_);
  w.u8(read_buffer_);
  w.u8(status_);
  w.u8(gg_cram_latch_);
  w.u8(line_counter_);
  w.u8(vscroll_latch_);
  w.u8(v_counter_);
  w.u8(h_counter_);
  w.boolean(vint_pending_);
  w.boolean(hint_pending_);
}

void VdpM4::load(md::StateReader &r) {
  r.bytes(vram_);
  r.bytes(cram_);
  r.bytes(regs_);
  addr_ = r.u16();
  code_ = r.u8();
  latch_ = r.u8();
  control_pending_ = r.boolean();
  read_buffer_ = r.u8();
  status_ = r.u8();
  gg_cram_latch_ = r.u8();
  line_counter_ = r.u8();
  vscroll_latch_ = r.u8();
  v_counter_ = r.u8();
  h_counter_ = r.u8();
  vint_pending_ = r.boolean();
  hint_pending_ = r.boolean();
}

} // namespace tessera::sms
