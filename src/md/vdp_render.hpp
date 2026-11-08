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
  /// Power-on state of what carries from frame to frame (the line hook stays).
  void reset() noexcept {
    flags_ = {};
    field_ = 0;
    double_res_ = false;
    offscreen_ = {};
    capture_ = {};
    captured_ = {};
    latch_ = {};
    line_regs_ = {};
    line_cram_ = {};
  }
  /// Sizes the frame for the mode in force and clears it.
  void begin_frame(const Vdp &vdp);
  /// Field drawn in interlace mode 2 (0 even double-resolution lines, 1 odd);
  /// it alternates every frame.
  unsigned field() const noexcept { return field_; }
  /// Draws `line` (0 based). Lines outside the active display are ignored.
  void render_line(const Vdp &vdp, std::uint32_t line);
  /// Takes what gpgx reads at the start of `line`, before the CPU runs in it:
  /// the horizontal scroll of both planes, the VSRAM, the shadow/highlight
  /// bit, the scroll mode, plane size and window registers and (through
  /// Vdp::vram_at_line_start) the VRAM the planes and the window read. A
  /// write in that line (its horizontal interrupt) shows from the next one;
  /// CRAM, the name table bases, the backdrop and display enable are read
  /// when the line is drawn (measured, raster_bands 0 to 32).
  void latch_line(const Vdp &vdp, std::uint32_t line);

  /// LINE_REGS (tessera_obs_line_regs_v1) and LINE_CRAM entries of each
  /// line, filled when the line is drawn (measured on gpgx, raster_bands):
  /// the name table bases are the live ones, every other field the one at
  /// the start of the line, the window clip included (Vdp::window_clip); a line
  /// with the display off both at its start and when drawn keeps the entry it
  /// had (the arrays live across frames, as in the reference). Not in the
  /// savestate.
  using LineRegs = std::array<std::uint8_t, 32>;
  const std::array<LineRegs, 240> &line_regs() const noexcept {
    return line_regs_;
  }
  const std::array<std::array<std::uint16_t, 64>, 240> &
  line_cram() const noexcept {
    return line_cram_;
  }

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

  /// Sprites the renderer parsed in the frame being drawn (TP-202.1/.2, regions
  /// PARSED_SPRITES and SPRITE_OUTCOME). `sprites` keeps the leftovers of
  /// earlier frames past `count`, as the reference does. It only runs while
  /// enabled: nobody subscribed means no work. Not part of the savestate.
  struct ParsedSprite {
    std::uint16_t yr = 0; ///< y field of the table entry (10 or 11 bits)
    std::uint16_t xr = 0; ///< x field (9 bits)
    std::uint16_t attr = 0;
    std::uint8_t w = 0; ///< cells
    std::uint8_t h = 0;
    std::uint8_t sat_idx = 0;
    std::uint8_t chain_pos = 0;
  };
  /// Outcome bits of a slot, accumulated over the frame.
  static constexpr std::uint8_t kOutcomeParsed = 1;    ///< entered a line list
  static constexpr std::uint8_t kOutcomeDrawn = 2;     ///< a visible column
  static constexpr std::uint8_t kOutcomeLineFull = 4;  ///< line limit
  static constexpr std::uint8_t kOutcomePixelFull = 8; ///< pixel limit reached
  static constexpr std::uint8_t kOutcomeMasked = 16;   ///< masked by X = 0
  struct SpriteCapture {
    std::array<ParsedSprite, 80> sprites{};
    unsigned count = 0;
    std::array<std::uint8_t, 80> outcome{};
  };
  void set_sprite_capture(bool on) noexcept { capture_on_ = on; }
  const SpriteCapture &sprite_capture() const noexcept { return capture_; }

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
  /// Parse phase of `line` for the capture (see SpriteCapture).
  void capture_line(const Vdp &vdp, std::uint32_t line);
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
  bool capture_on_ = false;
  SpriteCapture capture_;
  std::array<bool, 80> captured_{};
  /// Start of line values (latch_line); used only for the line they belong
  /// to, so nothing carries over a savestate taken between frames.
  struct LineLatch {
    std::uint32_t line = ~0U;
    std::array<unsigned, 2> hscroll{};
    std::array<std::uint16_t, 40> vsram{};
    bool shadow = false;
    std::array<std::uint8_t, 32> regs{};
    std::array<std::uint16_t, 64> cram{};
    std::array<std::uint8_t, 2> window_clip{};
  };
  /// Register `index` as it was at the start of `line` (latch_line), or the
  /// live one when the line was not latched. The planes and the window take
  /// registers 11, 16, 17 and 18 from here (measured, raster_bands 29-31).
  std::uint8_t start_reg(const Vdp &vdp, std::uint32_t line,
                         unsigned index) const noexcept;
  LineLatch latch_;
  void capture_line_state(const Vdp &vdp, std::uint32_t line);
  std::array<LineRegs, 240> line_regs_{};
  std::array<std::array<std::uint16_t, 64>, 240> line_cram_{};
};

} // namespace tessera::md
