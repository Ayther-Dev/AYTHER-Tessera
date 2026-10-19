#pragma once
#include "public_client.hpp"

namespace tessera::cli {
struct ReplayTrace {
  Bytes bytes;
  Json json;
  std::uint64_t length{};
};
[[nodiscard]] ReplayTrace parse_trace(View input);
[[nodiscard]] ReplayTrace make_demo_trace(View content, View config,
                                          std::uint64_t frames,
                                          std::string_view scenario);
} // namespace tessera::cli
