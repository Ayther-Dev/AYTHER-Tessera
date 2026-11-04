// Per line renderer of the native Mega Drive VDP (spec 008, TP-105.1).
// Output is RGBA8 (one uint32 per pixel: R in the low byte, A in the high
// byte). A hook is called once per line right after it is drawn, which is the
// point where phase 2 attaches; nothing runs after the last line. Planes and
// sprites arrive with TP-105.3 onwards; today a line holds the backdrop.
#pragma once
#include "md/vdp.hpp"

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

/// CRAM colour (0000BBB0GGG0RRR0) as RGBA8. The 3 to 8 bit conversion is
/// provisional until TP-105.2 takes the table from gpgx.
std::uint32_t colour_to_rgba(std::uint16_t cram) noexcept;

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
  std::vector<std::uint32_t> frame_;
  FrameSize size_;
  LineHook hook_;
};

} // namespace tessera::md
