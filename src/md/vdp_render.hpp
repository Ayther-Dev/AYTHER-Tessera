// Per line renderer of the native Mega Drive VDP (spec 008, TP-105.1).
// Output is RGBA8 (one uint32 per pixel: R in the low byte, A in the high
// byte). A hook is called once per line right after it is drawn, which is the
// point where phase 2 attaches; nothing runs after the last line. Planes and
// sprites arrive with TP-105.3 onwards; today a line holds the backdrop.
#pragma once
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
  /// Draws `line` (0 based). Lines outside the active display are ignored.
  void render_line(const Vdp &vdp, std::uint32_t line);

  const std::vector<std::uint32_t> &frame() const noexcept { return frame_; }
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
  std::vector<std::uint32_t> frame_;
  FrameSize size_;
  LineHook hook_;
};

} // namespace tessera::md
