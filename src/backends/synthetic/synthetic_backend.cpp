#include "backends/synthetic/synthetic_backend.hpp"

#include "domain/result.hpp"
#include "domain/wire.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace tessera {
namespace {

constexpr std::string_view minimal_id = "synthetic.minimal";
constexpr std::string_view complete_id = "synthetic.complete";
constexpr std::array<std::uint8_t, 8> content_magic{'T', 'S', 'S', 'Y',
                                                    'N', '0', '0', '1'};
constexpr std::uint32_t width = 16;
constexpr std::uint32_t height = 16;
constexpr std::uint32_t samples_per_frame = 800;
constexpr std::uint64_t ticks_per_frame = 1000;
constexpr std::uint64_t minimal_capabilities = 0x120F;
constexpr std::uint64_t complete_capabilities = 0x1FFF;
constexpr std::uint64_t supported_regions = 31;
using Palette = std::array<std::array<std::uint8_t, 4>, width * height>;

std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                       std::size_t offset) noexcept {
  std::uint32_t result = 0;
  for (unsigned byte = 0; byte < 4; ++byte) {
    result |= static_cast<std::uint32_t>(bytes[offset + byte]) << (byte * 8);
  }
  return result;
}

void append_le(Bytes &out, std::uint64_t value, unsigned byte_count) {
  for (unsigned byte = 0; byte < byte_count; ++byte) {
    out.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
  }
}

void append_field(Bytes &out, std::uint32_t tag, std::uint32_t encoding,
                  std::span<const std::uint8_t> value) {
  append_le(out, tag, 4);
  append_le(out, encoding, 4);
  append_le(out, value.size(), 8);
  out.insert(out.end(), value.begin(), value.end());
}

void append_integer_field(Bytes &out, std::uint32_t tag, std::uint32_t encoding,
                          std::uint64_t value, unsigned byte_count) {
  append_le(out, tag, 4);
  append_le(out, encoding, 4);
  append_le(out, byte_count, 8);
  append_le(out, value, byte_count);
}

std::uint32_t input_mask(const InputState &inputs) noexcept {
  std::uint32_t mask = 0;
  for (unsigned bit = 0; bit < inputs.size(); ++bit) {
    mask |= static_cast<std::uint32_t>(inputs[bit]) << bit;
  }
  return mask;
}

Palette make_palette(std::uint32_t mask, std::uint32_t accumulator,
                     const std::array<std::uint8_t, 16> &persistent,
                     std::uint64_t frame) noexcept {
  Palette palette{};
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      palette[x + width * y] = {
          static_cast<std::uint8_t>(x + mask),
          static_cast<std::uint8_t>(y + accumulator),
          static_cast<std::uint8_t>(persistent[(x + y) % 16] + frame), 255};
    }
  }
  return palette;
}

RegionData region(std::uint32_t kind, std::uint32_t encoding,
                  std::uint32_t element_bytes, std::uint32_t element_count,
                  Bytes data) {
  return {kind,          encoding, 0, element_bytes,
          element_count, 0,        0, std::move(data)};
}

} // namespace

Content parse_synthetic_content(std::span<const std::uint8_t> content) {
  require(content.size() == 24, TESSERA_INVALID_CONTENT);
  require(
      std::equal(content_magic.begin(), content_magic.end(), content.begin()),
      TESSERA_INVALID_CONTENT);
  Content result{read_u32(content, 8), read_u32(content, 12),
                 read_u32(content, 16)};
  require(result.profile == 1 || result.profile == 2, TESSERA_INVALID_CONTENT);
  require((result.flags & ~std::uint32_t{1}) == 0 && read_u32(content, 20) == 0,
          TESSERA_INVALID_CONTENT);
  return result;
}

SyntheticBackend::SyntheticBackend(
    std::string_view id, std::span<const std::uint8_t> content,
    std::span<const std::uint8_t> initial_persistent_memory) {
  const auto parsed = parse_synthetic_content(content);
  require((parsed.profile == 1 && id == minimal_id) ||
              (parsed.profile == 2 && id == complete_id),
          TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_CREATE);
  require(initial_persistent_memory.size() == persistent_.size(),
          TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_CREATE);
  profile_ = parsed.profile;
  accumulator_ = parsed.initial_value;
  audio_phase_ = parsed.initial_value % 256;
  std::copy(initial_persistent_memory.begin(), initial_persistent_memory.end(),
            persistent_.begin());
  // Reset fault injection belongs to the session harness, not the emulated
  // machine.
}

std::string_view SyntheticBackend::backend_id() const noexcept {
  return profile_ == 1 ? minimal_id : complete_id;
}

std::uint64_t SyntheticBackend::capabilities() const noexcept {
  return profile_ == 1 ? minimal_capabilities : complete_capabilities;
}

std::uint64_t SyntheticBackend::frame() const noexcept { return frame_; }
std::uint64_t SyntheticBackend::tick() const noexcept {
  return frame_ * ticks_per_frame;
}
const InputState &SyntheticBackend::latched_inputs() const noexcept {
  return latched_;
}
const Bytes &SyntheticBackend::pixels() const noexcept { return pixels_; }
const Bytes &SyntheticBackend::pcm() const noexcept { return pcm_; }

void SyntheticBackend::step(const InputState &inputs) {
  for (const auto value : inputs) {
    require(value == 0 || value == 1, TESSERA_INVALID_ARGUMENT,
            TESSERA_PHASE_STEP);
  }
  require(frame_ < std::numeric_limits<std::uint64_t>::max() / ticks_per_frame,
          TESSERA_BACKEND_FAILURE, TESSERA_PHASE_STEP);
  const auto mask = input_mask(inputs);
  const auto next_frame = frame_ + 1;
  const auto next_accumulator = accumulator_ + std::uint32_t{1} + mask;
  auto next_persistent = persistent_;
  for (std::size_t index = 0; index < next_persistent.size(); ++index) {
    next_persistent[index] = static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(next_persistent[index]) + 1 +
        static_cast<std::uint32_t>(inputs[index % inputs.size()]));
  }

  Bytes next_pixels(width * height * 4);
  Bytes next_pcm;
  next_pcm.reserve(samples_per_frame * 4);
  const auto palette =
      make_palette(mask, next_accumulator, next_persistent, next_frame);
  const auto sprite_x = mask & 7U;
  const auto sprite_y = (mask >> 3) & 7U;
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const auto palette_index =
          x == sprite_x && y == sprite_y ? 255U : x + width * y;
      const auto offset = static_cast<std::size_t>((x + width * y) * 4);
      std::copy(palette[palette_index].begin(), palette[palette_index].end(),
                next_pixels.begin() + static_cast<std::ptrdiff_t>(offset));
    }
  }

  const auto stride = 1 + (mask & 15U);
  const auto amplitude = static_cast<std::int32_t>(1000 + 4 * mask);
  for (std::uint32_t sample = 0; sample < samples_per_frame; ++sample) {
    const auto phase = (audio_phase_ + sample * stride) % 256;
    const auto left = phase < 128 ? amplitude : -amplitude;
    append_le(next_pcm, static_cast<std::uint16_t>(left), 2);
    append_le(next_pcm, static_cast<std::uint16_t>(-left), 2);
  }
  const auto next_phase = (audio_phase_ + samples_per_frame * stride) % 256;

  // All validation and allocation precede this non-throwing commit.
  accumulator_ = next_accumulator;
  persistent_ = next_persistent;
  audio_phase_ = next_phase;
  frame_ = next_frame;
  latched_ = inputs;
  pixels_.swap(next_pixels);
  pcm_.swap(next_pcm);
}

Bytes SyntheticBackend::machine_record() const {
  Bytes result;
  result.reserve(72);
  append_integer_field(result, 1, 1, accumulator_, 4);
  append_field(result, 2, 5, persistent_);
  append_integer_field(result, 3, 1, audio_phase_, 4);
  return result;
}

void SyntheticBackend::restore_record(std::span<const std::uint8_t> encoded,
                                      std::uint64_t frame,
                                      const InputState &inputs) {
  const auto fields = wire::record(encoded, TESSERA_CHECKPOINT_INVALID,
                                   TESSERA_PHASE_RESTORE);
  require(fields.size() == 3, TESSERA_CHECKPOINT_INVALID);
  const auto accumulator = static_cast<std::uint32_t>(
      wire::read(wire::get(fields, 0, 1, TESSERA_ENCODING_U32, 4,
                           TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE),
                 0, 4));
  const auto persistent = wire::get(fields, 1, 2, TESSERA_ENCODING_BYTES, 16,
                                    TESSERA_CHECKPOINT_INVALID,
                                    TESSERA_PHASE_RESTORE);
  const auto phase = wire::read(
      wire::get(fields, 2, 3, TESSERA_ENCODING_U32, 4,
                TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE),
      0, 4);
  require(phase < 256 && frame < UINT64_MAX / ticks_per_frame &&
              std::all_of(inputs.begin(), inputs.end(),
                          [](auto value) { return value == 0 || value == 1; }),
          TESSERA_CHECKPOINT_INVALID);
  accumulator_ = accumulator;
  std::copy(persistent.begin(), persistent.end(), persistent_.begin());
  audio_phase_ = static_cast<std::uint32_t>(phase);
  frame_ = frame;
  latched_ = inputs;
  pixels_.clear();
  pcm_.clear();
}

std::vector<RegionData> SyntheticBackend::regions(std::uint64_t mask) const {
  require(profile_ == 2, TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_READ);
  require((mask & ~supported_regions) == 0, TESSERA_UNSUPPORTED_CAPABILITY,
          TESSERA_PHASE_READ);
  if (mask == 0)
    return {};
  require(frame_ != 0, TESSERA_WRONG_STATE, TESSERA_PHASE_READ);
  std::vector<RegionData> result;
  result.reserve(5);
  if ((mask & 1) != 0) {
    result.push_back(
        region(1, 5, 1, 16, Bytes(persistent_.begin(), persistent_.end())));
  }
  if ((mask & 2) != 0) {
    const auto palette =
        make_palette(input_mask(latched_), accumulator_, persistent_, frame_);
    Bytes data;
    data.reserve(width * height * 4);
    for (const auto &colour : palette)
      data.insert(data.end(), colour.begin(), colour.end());
    result.push_back(region(2, 5, 4, width * height, std::move(data)));
  }
  if ((mask & 4) != 0) {
    const auto latched_mask = input_mask(latched_);
    Bytes data;
    data.reserve(8);
    append_le(data, latched_mask & 7U, 2);
    append_le(data, (latched_mask >> 3) & 7U, 2);
    append_le(data, 255, 2);
    append_le(data, 1, 2);
    result.push_back(region(3, 5, 8, 1, std::move(data)));
  }
  if ((mask & 8) != 0) {
    Bytes data;
    data.reserve(80);
    append_integer_field(data, 1, 1, width, 4);
    append_integer_field(data, 2, 1, height, 4);
    append_integer_field(data, 3, 1, 1, 4);
    append_integer_field(data, 4, 1, 1, 4);
    result.push_back(region(4, 11, 80, 1, std::move(data)));
  }
  if ((mask & 16) != 0) {
    Bytes data;
    data.reserve(89);
    append_integer_field(data, 1, 3, frame_, 8);
    append_integer_field(data, 2, 3, (frame_ - 1) * ticks_per_frame, 8);
    append_integer_field(data, 3, 3, tick(), 8);
    append_integer_field(data, 4, 7, 1, 1);
    result.push_back(region(5, 11, 89, 1, std::move(data)));
  }
  return result;
}

std::unique_ptr<IEmulatedSystem> make_synthetic_backend(
    std::string_view id, std::span<const std::uint8_t> content,
    std::span<const std::uint8_t> initial_persistent_memory) {
  return std::make_unique<SyntheticBackend>(id, content,
                                            initial_persistent_memory);
}

} // namespace tessera
