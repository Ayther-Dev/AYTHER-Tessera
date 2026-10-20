#pragma once

#include "systems/emulated_system.hpp"
#include <tessera/tessera.h>
#include <filesystem>

namespace tessera::persistence {

enum class StoreFault {
  none,
  read_denied,
  write_denied,
  write_no_space,
  partial_write_interruption,
  publish_denied
};

struct StoreResult {
  tessera_status code{TESSERA_OK};
  std::uint32_t phase{TESSERA_PHASE_NONE};
  std::uint32_t os_domain{TESSERA_OS_NONE};
  std::uint32_t os_code{};
  [[nodiscard]] bool ok() const noexcept { return code == TESSERA_OK; }
};

[[nodiscard]] StoreResult save_checkpoint(const std::filesystem::path &path,
                                          std::span<const std::uint8_t> bytes,
                                          StoreFault fault = StoreFault::none);
[[nodiscard]] StoreResult load_checkpoint(const std::filesystem::path &path,
                                          Bytes &out,
                                          StoreFault fault = StoreFault::none);

} // namespace tessera::persistence
