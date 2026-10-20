#pragma once

#include <cstdint>

namespace tessera::allocation_metrics {
struct Snapshot {
  std::uint64_t calls;
  std::uint64_t bytes;
};
Snapshot snapshot() noexcept;
} // namespace tessera::allocation_metrics
