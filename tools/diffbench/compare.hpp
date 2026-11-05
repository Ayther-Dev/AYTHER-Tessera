// Comparadores del banco diferencial (spec 008, RF-7.2, D-3).
// Plano 1: framebuffer. Plano 2: escrituras de audio. Regiones: byte a byte.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace diffbench {

struct Frame {
  uint32_t width = 0, height = 0;
  std::vector<uint32_t> pixels; // XRGB8888, fila a fila
};

struct PixelDivergence {
  uint64_t frame = 0;
  uint32_t line = 0, x = 0;
  uint32_t color_a = 0, color_b = 0; // 24 bits
};

// Primera divergencia de color de 24 bits (el byte alto no cuenta).
inline std::optional<PixelDivergence>
compare_frame(uint64_t frame, const Frame &a, const Frame &b) {
  if (a.width != b.width || a.height != b.height ||
      a.pixels.size() != b.pixels.size())
    return PixelDivergence{frame, 0, 0, 0, 0};
  for (size_t i = 0; i < a.pixels.size(); ++i) {
    const uint32_t ca = a.pixels[i] & 0xFFFFFF, cb = b.pixels[i] & 0xFFFFFF;
    if (ca != cb)
      return PixelDivergence{frame, static_cast<uint32_t>(i / a.width),
                             static_cast<uint32_t>(i % a.width), ca, cb};
  }
  return std::nullopt;
}

struct AudioWrite {
  uint8_t chip = 0;
  uint16_t addr = 0;
  uint8_t data = 0;
  uint32_t cycle = 0;
};

struct AudioDivergence {
  size_t index = 0;
};

// Secuencia (chip, addr, data); el ciclo solo cuenta si `compare_cycle`.
inline std::optional<AudioDivergence>
compare_audio_writes(const std::vector<AudioWrite> &a,
                     const std::vector<AudioWrite> &b, bool compare_cycle) {
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i)
    if (a[i].chip != b[i].chip || a[i].addr != b[i].addr ||
        a[i].data != b[i].data || (compare_cycle && a[i].cycle != b[i].cycle))
      return AudioDivergence{i};
  if (a.size() != b.size())
    return AudioDivergence{n};
  return std::nullopt;
}

struct PcmStats {
  int peak = 0;
  double rms = 0;
};

// Métrica informativa (§5.2): nunca decide el resultado del banco.
inline PcmStats pcm_stats(const std::vector<int16_t> &pcm) {
  PcmStats s;
  double sum = 0;
  for (const auto v : pcm) {
    s.peak = std::max(s.peak, std::abs(static_cast<int>(v)));
    sum += static_cast<double>(v) * v;
  }
  s.rms = pcm.empty() ? 0 : std::sqrt(sum / static_cast<double>(pcm.size()));
  return s;
}

struct RegionDivergence {
  size_t offset = 0;
  uint8_t value_a = 0, value_b = 0;
};

// Byte a byte; `mask` vacía = todos los bits. Distinta longitud diverge.
inline std::optional<RegionDivergence>
compare_region(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b,
               const std::vector<uint8_t> &mask) {
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    const uint8_t m = mask.empty() ? 0xFF : mask[i % mask.size()];
    if ((a[i] & m) != (b[i] & m))
      return RegionDivergence{i, a[i], b[i]};
  }
  if (a.size() != b.size())
    return RegionDivergence{n, 0, 0};
  return std::nullopt;
}

} // namespace diffbench
