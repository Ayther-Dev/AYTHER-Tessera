#pragma once

#include <clownmdemu.h>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace clown_state_codec {
// The caller reconstructs the same ROM/configuration/callbacks before decoding.
// Only emulated machine fields are applied; host PCM staging belongs to the caller.
[[nodiscard]] std::vector<std::uint8_t> encode_machine(const ClownMDEmu& machine);
void decode_machine(ClownMDEmu& machine, std::span<const std::uint8_t> encoded);
[[nodiscard]] std::string_view schema_identity() noexcept;
} // namespace clown_state_codec
