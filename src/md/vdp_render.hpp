// Per line renderer of the native Mega Drive VDP (spec 008, TP-105.1).
// Output is RGBA8 (one uint32 per pixel: R in the low byte, A in the high
// byte). A hook is called once per line right after it is drawn, which is the
// point where phase 2 attaches; nothing runs after the last line. Planes and
// sprites arrive with TP-105.3 onwards; today a line holds the backdrop.
#pragma once
#include "md/state_io.hpp"
#include "md/vdp.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace tessera::md {

struct FrameSize {
  std::uint32_t width = 256;
  std::uint32_t height = 224;
};

/// Visible size for the current mode: H32 256 / H40 320, V28 224 / V30 240.
FrameSize frame_size(const Vdp &vdp) noexcept;

/// Mode 4 CRAM entry (as kept in CRAM) to its raw 6 bit value and to 565.
unsigned mode4_raw(std::uint16_t cram) noexcept;
std::uint16_t mode4_565(unsigned raw) noexcept;

/// CRAM colour (0000BBB0GGG0RRR0) as RGBA8, the way a host sees the 565 frame
/// of gpgx (TP-105.2); `highlight` is the same rule, not yet checked on gpgx.
enum class Shade : std::uint8_t { normal, shadow, highlight };
std::uint16_t colour_to_565(std::uint16_t cram, Shade shade) noexcept;
/// PALETTE region (RF-3.3): 256 RGB565 entries as gpgx serves them.
std::array<std::uint16_t, 256> palette_565(const Vdp &vdp) noexcept;
std::uint32_t colour_to_rgba(std::uint16_t cram,
                             Shade shade = Shade::normal) noexcept;

class Renderer {
public:
  using LineHook =
      std::function<void(std::uint32_t line, std::span<const std::uint32_t>)>;

  void set_line_hook(LineHook hook) { hook_ = std::move(hook); }
  /// Sizes the frame for the mode in force and clears it.
  void begin_frame(const Vdp &vdp);
  /// Field drawn in interlace mode 2 (0 even double-resolution lines, 1 odd);
  /// it alternates every frame.
  unsigned field() const noexcept { return field_; }
  /// Draws `line` (0 based). Lines outside the active display are ignored.
  void render_line(const Vdp &vdp, std::uint32_t line);

  /// Sprite flags of the lines drawn since the last call (status register bits
  /// 6 overflow and 5 collision); reading clears them.
  struct SpriteFlags {
    bool overflow = false;
    bool collision = false;
  };
  SpriteFlags take_sprite_flags() noexcept {
    const SpriteFlags flags = flags_;
    flags_ = {};
    return flags;
  }

  const std::vector<std::uint32_t> &frame() const noexcept { return frame_; }

  /// Savestate: what carries from one frame to the next (the frame itself is
  /// drawn again).
  void save(StateWriter &w) const;
  void load(StateReader &r);
  FrameSize size() const noexcept { return size_; }

private:
  struct Pixel {
    std::uint8_t index = 0; ///< 0..63 CRAM entry; colour 0 of a line is clear
    bool high = false;      ///< priority bit of the tile
    bool opaque = false;    ///< colour index of the tile pixel is not 0
  };
  unsigned hscroll_value(const Vdp &vdp, unsigned plane,
                         std::uint32_t line) const;
  /// Overwrites plane A with the window where it covers the line.
  void window_line(const Vdp &vdp, std::uint32_t line,
                   std::span<Pixel> a) const;
  /// Fills `out` (one entry per visible column) with plane A (0) or B (1).
  void plane_line(const Vdp &vdp, unsigned plane, std::uint32_t line,
                  std::span<Pixel> out) const;
  /// Sprites of `line` (TP-105.5): walks the link list of the sprite table and
  /// fills `out` (one entry per visible column) with the first opaque pixel
  /// of each column, in list order.
  void sprite_line(const Vdp &vdp, std::uint32_t line, std::span<Pixel> out);
  /// Mode 4 (TP-105.10): background, sprites and CRAM of the SMS VDP.
  void render_line_m4(const Vdp &vdp, std::uint32_t line);
  std::vector<std::uint32_t> frame_;
  FrameSize size_;
  LineHook hook_;
  SpriteFlags flags_;
  unsigned field_ = 0;
  /// Double resolution (interlace mode 2): cells are 8x16, the line that
  /// planes and sprites draw is 2 * line + field.
  bool double_res_ = false;
  /// Sprite pixels left outside the visible columns (x from -128 to 383):
  /// gpgx never clears them, so a later opaque sprite pixel on the same
  /// column counts as a collision (measured, sprite_limits 17 to 25).
  std::array<bool, 512> offscreen_{};
};

} // namespace tessera::md
