#pragma once
#include "support/fx01.hpp"
namespace independent_oracle {
struct Digests {
  std::string state, video, audio;
};
Digests frame(bool complete, bool action, std::uint64_t frame);
} // namespace independent_oracle
