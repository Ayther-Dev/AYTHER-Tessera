#pragma once

#include "systems/emulated_system.hpp"

#include <span>
#include <string_view>

namespace tessera::persistence {

struct ParsedState {
  std::uint64_t completed_frames{}, logical_tick{};
  InputState inputs{};
  Bytes machine;
};

struct CheckpointData {
  Bytes state;
  Bytes trace_sha;
  std::uint64_t trace_length{}, next_replay_frame{};
  Bytes initial_sha;
};

[[nodiscard]] Bytes encode_checkpoint(std::span<const std::uint8_t> state,
                                      std::span<const std::uint8_t> trace,
                                      std::uint64_t next_replay_frame,
                                      std::span<const std::uint8_t> initial_sha);
[[nodiscard]] CheckpointData
decode_checkpoint(std::span<const std::uint8_t> bytes);
[[nodiscard]] ParsedState decode_state(
    std::span<const std::uint8_t> bytes, std::string_view backend,
    std::span<const std::uint8_t> content_sha,
    std::span<const std::uint8_t> build_id,
    std::span<const std::uint8_t> config_sha, std::uint64_t ticks_per_frame);

} // namespace tessera::persistence
