#pragma once
#include "domain/wire.hpp"
namespace tessera {
struct Detection {
  tessera_status status{};
  tessera_detection_v1 info{};
  std::vector<tessera_candidate_v1> candidates;
};
class SystemDetector {
public:
  [[nodiscard]] static Detection detect(wire::View,
                                        std::uint32_t system_override);
};
} // namespace tessera
