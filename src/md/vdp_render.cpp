#include "md/vdp_render.hpp"

#include <algorithm>

namespace tessera::md {

FrameSize frame_size(const Vdp &vdp) noexcept {
  FrameSize size;
  size.width = (vdp.reg(12) & 0x81U) != 0 ? 320U : 256U;
  size.height = (vdp.reg(1) & 0x08U) != 0 ? 240U : 224U;
  return size;
}

std::uint32_t colour_to_rgba(std::uint16_t cram) noexcept {
  const auto scale = [](unsigned level) { return level * 255U / 7U; };
  const std::uint32_t red = scale((cram >> 1) & 7U);
  const std::uint32_t green = scale((cram >> 5) & 7U);
  const std::uint32_t blue = scale((cram >> 9) & 7U);
  return red | (green << 8) | (blue << 16) | 0xFF000000U;
}

void Renderer::begin_frame(const Vdp &vdp) {
  size_ = frame_size(vdp);
  frame_.assign(std::size_t{size_.width} * size_.height, 0xFF000000U);
}

void Renderer::render_line(const Vdp &vdp, std::uint32_t line) {
  if (line >= size_.height)
    return;
  const std::uint32_t backdrop = colour_to_rgba(vdp.cram()[vdp.reg(7) & 0x3FU]);
  const auto row = std::span<std::uint32_t>(frame_).subspan(
      std::size_t{line} * size_.width, size_.width);
  std::fill(row.begin(), row.end(), backdrop);
  if (hook_)
    hook_(line, row);
}

} // namespace tessera::md
