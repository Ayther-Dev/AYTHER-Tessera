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
/// Back from a frame pixel to the 565 it was widened from (exact: the
/// widening replicates bits).
constexpr std::uint16_t rgba_to_565(std::uint32_t c) noexcept {
  return static_cast<std::uint16_t>(((c >> 3) & 0x1FU) << 11 |
                                    ((c >> 10) & 0x3FU) << 5 |
                                    ((c >> 19) & 0x1FU));
}

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
    line_regs_.assign(240, LineRegs{});
    line_cram_.assign(240, LineCram{});
    line_cells_.assign(240, LineCells{});
    attribution_.clear();
    attribution_drawn_ = false;
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
  using LineCram = std::array<std::uint16_t, 64>;
  // The per-line tables live on the heap (about 100 KB): a System is often
  // built on the stack.
  const std::vector<LineRegs> &line_regs() const noexcept { return line_regs_; }
  const std::vector<LineCram> &line_cram() const noexcept { return line_cram_; }
  /// LINE_CELLS entries (tessera_obs_line_cells_v1, 261 bytes padded to
  /// 264), filled with LINE_REGS (capture_line_cells). Not in the savestate.
  using LineCells = std::array<std::uint8_t, 264>;
  const std::vector<LineCells> &line_cells() const noexcept {
    return line_cells_;
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
  /// ATTRIBUTION (RF-3.4): one byte per pixel of the drawn frame, filled
  /// while on (ABI: bits 7-6 layer 0 backdrop / 1 B / 2 A / 3 window, bit 5
  /// priority of the winning background cell, bits 4-3 palette line, bits
  /// 2-1 shadow 1 / highlight 2, bit 0 sprite). Not in the savestate.
  void set_attribution(bool on) noexcept { attribution_on_ = on; }
  const std::vector<std::uint8_t> &attribution() const noexcept {
    return attribution_;
  }
  /// False until a line went through the merge since power on: gpgx serves
  /// ATTRIBUTION with 0 bytes until then (measured, Streets of Rage 2 and
  /// Gunstar Heroes at frame 120, display never on yet); later frames with
  /// the display off keep the old bytes (Phantasy Star IV 240, Gunstar Heroes
  /// 3294).
  bool attribution_drawn() const noexcept { return attribution_drawn_; }
  const SpriteCapture &sprite_capture() const noexcept { return capture_; }

  const std::vector<std::uint32_t> &frame() const noexcept { return frame_; }
  /// recompose_frame (TP-205.1): the last frame drawn again, every line from
  /// the VDP state as it is now (after the frame), with no raster change. It
  /// works on copies: neither the machine nor this renderer changes. `size`
  /// is the size of the picture returned: the emitted frame's. Empty in
  /// interlace mode 2, which gpgx refuses (measured, modes_sweep 23 to 27).
  /// The flags of the call change nothing (measured, every bit). On the frame
  /// where the width goes from H40 to H32, columns 256 to 319 are not the ones
  /// gpgx returns (the open question of raster_bands 7).
  std::vector<std::uint32_t> recompose(const Vdp &vdp, FrameSize &size) const;

  /// LAYER_MASK bits (the Engine's AYTHER_LAYER_*): planes A and B, the
  /// window and the sprites. A hidden layer draws nothing.
  static constexpr std::uint8_t kLayerA = 1;
  static constexpr std::uint8_t kLayerB = 2;
  static constexpr std::uint8_t kLayerWindow = 4;
  static constexpr std::uint8_t kLayerSprites = 8;
  static constexpr std::uint8_t kLayerPlanes = 7;
  static constexpr std::uint8_t kLayerAll = 15;
  /// The render controls in force (RENDER_CONTROLS): LAYER_MASK and
  /// LAYER_DIM, which darkens the whole frame to a quarter (each channel
  /// >> 2, measured on gpgx after the mask).
  void set_controls(std::uint8_t mask, bool dim) noexcept {
    layer_mask_ = mask;
    dim_ = dim;
  }
  std::uint8_t layer_mask() const noexcept { return layer_mask_; }
  bool dim() const noexcept { return dim_; }
  /// recompose_multilayer (TP-205.2): plane A, plane B, window, sprites and
  /// the composite, each a frame like recompose's. LAYER_DIM darkens all
  /// five; LAYER_MASK none (the composite shows every layer). With
  /// `replay` (RASTER_TRACKING) the raster journal is replayed over the
  /// final state. False in interlace mode 2, in Mode 4 and when `replay`
  /// meets a journal that lost events.
  bool recompose_layers(const Vdp &vdp, bool isolate, bool replay,
                        std::array<std::vector<std::uint32_t>, 5> &layers,
                        FrameSize &size) const;

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
    bool collided = false;  ///< sprite pixel met by a later opaque sprite
    bool window = false;    ///< drawn by the window (it replaces plane A)
  };
  /// Clears the pixels of the layers LAYER_MASK hides.
  void hide_layers(std::span<Pixel> a, std::span<const Pixel> plane_a,
                   std::span<Pixel> b, std::span<Pixel> sp, bool whole_line,
                   unsigned shift, bool shadow) const noexcept;
  /// The last frame drawn again from `vdp` with `layer_mask` (recompose);
  /// `multilayer` for the passes of recompose_layers, `replay` to replay the
  /// raster journal.
  std::vector<std::uint32_t> redraw(const Vdp &vdp, std::uint8_t layer_mask,
                                    bool multilayer, bool replay,
                                    FrameSize &size) const;
  std::uint8_t layer_mask_ = kLayerAll;
  bool dim_ = false;
  /// Plane A of the last line the window did not cover whole: gpgx shows it
  /// again on such lines with plane A and the window shown and plane B
  /// hidden (measured, modes_sweep 8, 9, 12: line 223 of the frame before).
  std::vector<Pixel> last_plane_a_;
  /// Pixels of the line being drawn that a sprite won (LAYER_DIM spares them).
  std::array<bool, 320> line_sprite_{};
  std::array<bool, 320> emitted_sprite_{}; ///< line_sprite_ of the last line
  bool multilayer_ = false;                ///< a pass of recompose_layers
  bool redrawing_ = false;                 ///< recompose or recompose_layers
  /// Journal replay over a final state without shadow/highlight.
  bool replay_without_shadow_ = false;
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
  void render_line_m5(const Vdp &vdp, std::uint32_t line);
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
  bool attribution_on_ = false;
  bool attribution_drawn_ = false;
  std::vector<std::uint8_t> attribution_;
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
  std::vector<LineRegs> line_regs_ = std::vector<LineRegs>(240);
  std::vector<LineCram> line_cram_ = std::vector<LineCram>(240);
  void capture_line_cells(const Vdp &vdp, std::uint32_t line);
  std::uint8_t attribution_byte(const Pixel &pa, const Pixel &pb, unsigned x,
                                bool shadow_mode, bool window_line,
                                bool from_sprite) const noexcept;
  std::vector<LineCells> line_cells_ = std::vector<LineCells>(240);
};

} // namespace tessera::md
