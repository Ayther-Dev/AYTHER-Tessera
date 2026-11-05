// Host libretro mÃ­nimo del banco diferencial (spec 008, RF-7.2, TP-005.2).
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
  std::vector<uint8_t> bytes; ///< contents read, for --region-out
};

using TraceFrame = std::array<int32_t, 8>; // up down left right A B C start

struct RunResult {
  int peak = 0;                       // pico absoluto del PCM entregado
  std::vector<RegionDump> regions;    // ABI 1.10, si se pidiÃ³ el volcado
  std::vector<uint64_t> frame_hashes; // FNV-1a 64 del framebuffer XRGB8888
  std::vector<uint8_t> save_ram;      // RETRO_MEMORY_SAVE_RAM al final
  std::vector<uint8_t> work_ram;      // RETRO_MEMORY_SYSTEM_RAM al final
  std::vector<uint8_t>
      last_frame; // ancho, alto (u32 LE) y R,G,B por pixel del ultimo frame
  uint32_t width = 0, height = 0;
  std::string error;
};

// Hash FNV-1a 64 canónico de un frame: tripletas R,G,B por píxel, fila a fila,
// sin relleno. Ambos lados lo usan para que los hashes sean comparables.
inline uint64_t fnv_rgb(uint64_t h, uint8_t r, uint8_t g, uint8_t b) {
  for (const uint8_t v : {r, g, b}) {
    h ^= v;
    h *= 1099511628211ull;
  }
  return h;
}
inline constexpr uint64_t fnv_seed = 1469598103934665603ull;

#ifdef TESSERA_DIFFBENCH_WITH_TESSERA
// Ejecuta el backend de referencia Clown por la API pública de Tessera.
RunResult run_tessera(const std::string &backend_id,
                      const std::string &rom_path, unsigned frames,
                      const std::vector<TraceFrame> *trace = nullptr);
#endif

// Escritura de control sobre una region despues de `frame` frames
// (`--poke <region>:<offset hex>:<bytes hex>:<frame>`).
struct Poke {
  uint32_t region = 0, offset = 0;
  std::vector<uint8_t> data;
  unsigned frame = 0;
  int32_t status = 0; ///< resultado de write_control
};

// Mando fijo durante toda la ejecucion (TP-108): 12 bits en el orden de la
// Mega Drive, arriba, abajo, izquierda, derecha, B, C, A, start, X, Y, Z, mode;
// uno por puerto. Sin efecto si hay traza de entradas.
void set_pad_masks(uint32_t port0, uint32_t port1);
// Dispositivo libretro que se pide para ambos puertos tras cargar el juego
// (0 = el del core por defecto; 0x101 = mando de 6 botones de gpgx).
void set_pad_device(unsigned device);
/// Value the core reads for an option (GET_VARIABLE); the rest keep defaults.
void set_core_option(const std::string &key, const std::string &value);

// Ejecuta `frames` frames con `rom` y devuelve un hash por frame.
RunResult run_libretro(const std::string &core_path,
                       const std::string &rom_path, unsigned frames,
                       bool dump_regions = false,
                       const std::vector<TraceFrame> *trace = nullptr,
                       Poke *poke = nullptr);

} // namespace diffbench
