// Host libretro mínimo del banco diferencial (spec 008, RF-7.2, TP-005.2).
// Carga un core por ruta; gpgx nunca se enlaza (RF-1.4).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace diffbench {

struct RunResult {
  std::vector<uint64_t> frame_hashes; // FNV-1a 64 del framebuffer XRGB8888
  uint32_t width = 0, height = 0;
  std::string error;
};

// Ejecuta `frames` frames con `rom` y devuelve un hash por frame.
RunResult run_libretro(const std::string &core_path,
                       const std::string &rom_path, unsigned frames);

} // namespace diffbench
