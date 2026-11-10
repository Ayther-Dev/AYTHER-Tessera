// Tabla de regiones de la ABI 1.10 (spec 008, TP-006.5). Medida por caja negra
// con el banco diferencial sobre gpgx r4: tamaños, flags y ids heredados.
#pragma once
#include "tessera/tessera.h"

#include <cstdint>

namespace tessera::observation {

struct RegionRow {
  std::uint32_t id, element_size, capacity, byte_size, access_flags,
      data_version, legacy_id;
};

inline constexpr RegionRow kRegions[] = {
    {1, 1, 65536, 65536, 49, 1, 3u},
    {2, 1, 128, 128, 17, 1, 256u},
    {3, 1, 32, 32, 17, 1, 257u},
    {4, 1, 128, 128, 17, 1, 263u},
    {5, 1, 1, 1, 19, 1, 258u},
    {6, 1, 16, 16, 19, 1, 259u},
    {7, 1, 512, 512, 19, 1, 260u},
    {8, 1, 3072, 3072, 19, 1, 261u},
    {9, 1, 1, 1, 17, 1, 262u},
    {10, 1, 1, 1, 19, 1, 264u},
    {11, 8, 8192, 65536, 29, 1, 265u},
    {12, 4, 1, 4, 29, 1, 266u},
    {13, 10, 128, 1280, 29, 1, 267u},
    {14, 1, 1, 1, 21, 1, 268u},
    {15, 4, 1, 4, 27, 1, 269u},
    {16, 4, 1, 4, 29, 1, 270u},
    {17, 1, 71680, 71680, 13, 1, 4294967295u},
    {18, 44, 1, 44, 9, 1, 4294967295u},
    {19, 1, 7192, 7192, 13, 1, 4294967295u},
    {20, 1, 152, 152, 13, 1, 4294967295u},
    {21, 1, 59160, 59160, 37, 1, 4294967295u},
    {22, 2064, 1, 2064, 13, 1, 4294967295u},
    {23, 56, 1, 56, 13, 1, 4294967295u},
    {24, 2, 256, 512, 9, 1, 4294967295u},
    {25, 1, 80, 80, 13, 1, 4294967295u},
    {26, 1, 8192, 8192, 11, 1, 271u},
};

/// Suscripción (TESSERA_OBS_SUB_*) que activa una región; 0 = se sirve siempre.
/// Medido sobre gpgx r4 (matriz en tests/fixtures/diffbench/
/// subscription-matrix.json): los controles 5 a 10 y 15, SYSTEM y Z80_RAM no
/// piden suscripción; PALETTE cuelga de VDP_MEMORY.
constexpr std::uint32_t region_gate(std::uint32_t id) noexcept {
  switch (id) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 24:
    return TESSERA_OBS_SUB_VDP_MEMORY;
  case 11:
  case 12:
    return TESSERA_OBS_SUB_AUDIO_WRITES;
  case 13:
  case 14:
  case 25:
    return TESSERA_OBS_SUB_SPRITE_CAPTURE;
  case 16:
  case 22:
    return TESSERA_OBS_SUB_RASTER_TRACKING;
  case 17:
    return TESSERA_OBS_SUB_ATTRIBUTION;
  case 19:
    return TESSERA_OBS_SUB_LINE_STATE;
  case 20:
    return TESSERA_OBS_SUB_LINE_CRAM;
  case 21:
    return TESSERA_OBS_SUB_LINE_CELLS;
  case 23:
    return TESSERA_OBS_SUB_FRAME_HASH;
  default:
    return 0;
  }
}

inline constexpr std::uint32_t kRegionCount =
    sizeof(kRegions) / sizeof(kRegions[0]);

/// TESSERA_OBS_REGION_WORK_RAM (TP-301.3, RF-3.1): not an ABI 1.10 row. The
/// 64 KiB work RAM word-swapped as gpgx's legacy RETRO_MEMORY_SYSTEM_RAM (2),
/// read and control write, served without subscription.
inline constexpr RegionRow kWorkRamRow{
    TESSERA_OBS_REGION_WORK_RAM,
    1,
    65536,
    65536,
    TESSERA_OBS_REGION_ACCESS_READ | TESSERA_OBS_REGION_ACCESS_CONTROL_WRITE |
        TESSERA_OBS_REGION_WORD_SWAPPED_LE,
    1,
    2u};

} // namespace tessera::observation
