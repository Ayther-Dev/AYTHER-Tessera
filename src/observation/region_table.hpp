// Tabla de regiones de la ABI 1.10 (spec 008, TP-006.5). Medida por caja negra
// con el banco diferencial sobre gpgx r4: tamaños, flags y ids heredados.
#pragma once
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

inline constexpr std::uint32_t kRegionCount =
    sizeof(kRegions) / sizeof(kRegions[0]);

} // namespace tessera::observation
