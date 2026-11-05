#pragma once

#include "systems/emulated_system.hpp"

#include <span>
#include <string_view>

namespace tessera::persistence {

/// Name of the machine record codec of a backend. A checkpoint carries it, and
/// a restore into a backend with another codec is refused as incompatible
/// (TESSERA_CHECKPOINT_INCOMPATIBLE): the Clown record (`TSCLOWN1`) and the
/// native one are not interchangeable (RF-4.1).
[[nodiscard]] constexpr std::string_view
codec_id(std::string_view backend) noexcept {
  if (backend == "clownmdemu")
    return "tessera.megadrive.clown";
  if (backend == "megadrive")
    return "tessera.megadrive.native";
  return "tessera.synthetic";
}

struct ParsedState {
  std::uint64_t completed_frames{}, logical_tick{};
  InputState inputs{};
  InputLayout input_layout = kLegacyInputLayout;
  Bytes machine;
};

struct CheckpointData {
  Bytes state;
  Bytes trace_sha;
  std::uint64_t trace_length{}, next_replay_frame{};
  Bytes initial_sha;
};

[[nodiscard]] Bytes encode_checkpoint(
    std::span<const std::uint8_t> state, std::span<const std::uint8_t> trace,
    std::uint64_t next_replay_frame, std::span<const std::uint8_t> initial_sha);
[[nodiscard]] CheckpointData
decode_checkpoint(std::span<const std::uint8_t> bytes);
[[nodiscard]] ParsedState
decode_state(std::span<const std::uint8_t> bytes, std::string_view backend,
             std::span<const std::uint8_t> content_sha,
             std::span<const std::uint8_t> build_id,
             std::span<const std::uint8_t> config_sha,
             std::uint64_t ticks_per_frame);

} // namespace tessera::persistence
