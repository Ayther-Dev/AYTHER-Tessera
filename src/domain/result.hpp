#pragma once
#include <cstdint>
#include <tessera/tessera.h>

namespace tessera {
struct Failure {
  tessera_status code;
  std::uint32_t phase{};
};
inline void require(bool condition,
                    tessera_status code = TESSERA_INVALID_ARGUMENT,
                    std::uint32_t phase = 0) {
  if (!condition)
    throw Failure{code, phase};
}
} // namespace tessera
