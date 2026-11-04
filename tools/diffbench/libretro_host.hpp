// Host libretro mínimo del banco diferencial (spec 008, RF-7.2, TP-005.2).
// Carga un core por ruta; gpgx nunca se enlaza (RF-1.4).
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace diffbench {

struct RegionDump {
  uint32_t id = 0, byte_size = 0, access = 0, element_size = 0, capacity = 0,
           data_version = 0, legacy_id = 0;
  int32_t query_status = 0, read_status = 0;
  uint32_t bytes_read = 0;
  uint64_t hash = 0;
};

using TraceFrame = std::array<int32_t, 8>; // up down left right A B C start

struct RunResult {
  int peak = 0;                       // pico absoluto del PCM entregado
  std::vector<RegionDump> regions;    // ABI 1.10, si se pidió el volcado
  std::vector<uint64_t> frame_hashes; // FNV-1a 64 del framebuffer XRGB8888
  uint32_t width = 0, height = 0;
  std::string error;
};

// Ejecuta `frames` frames con `rom` y devuelve un hash por frame.
RunResult run_libretro(const std::string &core_path,
                       const std::string &rom_path, unsigned frames,
                       bool dump_regions = false,
                       const std::vector<TraceFrame> *trace = nullptr);

} // namespace diffbench
