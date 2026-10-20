#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "backends/synthetic/synthetic_backend.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using tessera::Bytes;
using tessera::InputState;
constexpr std::string_view minimal_id = "synthetic.minimal";
constexpr std::string_view complete_id = "synthetic.complete";
constexpr std::array<std::uint8_t, 16> initial_memory{
    3, 20, 37, 54, 71, 88, 105, 122, 139, 156, 173, 190, 207, 224, 241, 2};

void append_u32(Bytes &out, std::uint32_t value) {
  for (unsigned byte = 0; byte < 4; ++byte) {
    out.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
  }
}

Bytes content(std::uint32_t profile, std::uint32_t initial = 7,
              std::uint32_t flags = 0, std::uint32_t reserved = 0) {
  Bytes result{'T', 'S', 'S', 'Y', 'N', '0', '0', '1'};
  for (const auto value : {profile, initial, flags, reserved})
    append_u32(result, value);
  return result;
}

std::uint64_t read_le(std::span<const std::uint8_t> bytes, std::size_t offset,
                      std::size_t count) {
  REQUIRE(offset <= bytes.size());
  REQUIRE(count <= bytes.size() - offset);
  REQUIRE(count <= 8);
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < count; ++index) {
    value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
  }
  return value;
}

struct MachineState {
  std::uint32_t accumulator{};
  std::array<std::uint8_t, 16> memory{};
  std::uint32_t phase{};
};

MachineState inspect_machine(const Bytes &bytes) {
  // Fixed offsets are independently derived from the three normative TLVs.
  REQUIRE(bytes.size() == 72);
  CHECK(read_le(bytes, 0, 4) == 1);
  CHECK(read_le(bytes, 4, 4) == 1);
  CHECK(read_le(bytes, 8, 8) == 4);
  CHECK(read_le(bytes, 20, 4) == 2);
  CHECK(read_le(bytes, 24, 4) == 5);
  CHECK(read_le(bytes, 28, 8) == 16);
  CHECK(read_le(bytes, 52, 4) == 3);
  CHECK(read_le(bytes, 56, 4) == 1);
  CHECK(read_le(bytes, 60, 8) == 4);
  MachineState result;
  result.accumulator = static_cast<std::uint32_t>(read_le(bytes, 16, 4));
  std::copy_n(bytes.begin() + 36, result.memory.size(), result.memory.begin());
  result.phase = static_cast<std::uint32_t>(read_le(bytes, 68, 4));
  return result;
}

InputState inputs(std::uint32_t mask) {
  InputState result{};
  for (unsigned index = 0; index < result.size(); ++index) {
    result[index] = static_cast<std::int32_t>((mask >> index) & 1U);
  }
  return result;
}

struct PrefixOracle {
  std::uint64_t frames{};
  std::uint64_t mask_sum{};
  std::uint64_t audio_step_sum{};
  std::array<std::uint64_t, 8> pressed_count{};

  void record(std::uint32_t mask) {
    ++frames;
    mask_sum += mask;
    audio_step_sum += 1 + (mask & 15U);
    for (unsigned bit = 0; bit < pressed_count.size(); ++bit) {
      pressed_count[bit] += (mask >> bit) & 1U;
    }
  }

  [[nodiscard]] MachineState state(std::uint32_t initial = 7) const {
    MachineState result;
    result.accumulator =
        static_cast<std::uint32_t>(initial + frames + mask_sum);
    for (std::size_t index = 0; index < result.memory.size(); ++index) {
      result.memory[index] = static_cast<std::uint8_t>(
          initial_memory[index] + frames + pressed_count[index % 8]);
    }
    result.phase =
        static_cast<std::uint32_t>((initial + 800 * audio_step_sum) % 256);
    return result;
  }
};

std::array<std::uint8_t, 4> palette_colour(std::uint32_t index,
                                           std::uint32_t mask,
                                           std::uint64_t frame,
                                           const MachineState &state) {
  const auto x = index % 16;
  const auto y = index / 16;
  return {static_cast<std::uint8_t>(x + mask),
          static_cast<std::uint8_t>(y + state.accumulator),
          static_cast<std::uint8_t>(state.memory[(x + y) % 16] + frame), 255};
}

void check_outputs(const tessera::IEmulatedSystem &backend,
                   const PrefixOracle &prefix, std::uint32_t mask,
                   std::uint32_t previous_phase) {
  const auto expected = prefix.state();
  const auto state = inspect_machine(backend.machine_record());
  CHECK(state.accumulator == expected.accumulator);
  CHECK(state.memory == expected.memory);
  CHECK(state.phase == expected.phase);
  CHECK(backend.frame() == prefix.frames);
  CHECK(backend.tick() == prefix.frames * 1000);
  CHECK(backend.latched_inputs() == inputs(mask));
  REQUIRE(backend.pixels().size() == 16 * 16 * 4);
  const auto sx = mask & 7U;
  const auto sy = (mask >> 3) & 7U;
  for (std::uint32_t y = 0; y < 16; ++y) {
    for (std::uint32_t x = 0; x < 16; ++x) {
      const auto index = x == sx && y == sy ? 255U : x + 16 * y;
      const auto colour = palette_colour(index, mask, prefix.frames, expected);
      const auto offset = static_cast<std::size_t>((x + 16 * y) * 4);
      for (std::size_t channel = 0; channel < colour.size(); ++channel) {
        CHECK(backend.pixels()[offset + channel] == colour[channel]);
      }
    }
  }
  REQUIRE(backend.pcm().size() == 800 * 4);
  const auto amplitude = static_cast<std::int32_t>(1000 + 4 * mask);
  const auto stride = 1 + (mask & 15U);
  std::uint64_t energy = 0;
  for (std::uint32_t sample = 0; sample < 800; ++sample) {
    const auto phase = (previous_phase + sample * stride) % 256;
    const auto left = phase < 128 ? amplitude : -amplitude;
    const auto offset = static_cast<std::size_t>(sample * 4);
    const auto actual_left = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(read_le(backend.pcm(), offset, 2)));
    const auto actual_right = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(read_le(backend.pcm(), offset + 2, 2)));
    CHECK(actual_left == left);
    CHECK(actual_right == -left);
    const auto wide_left = static_cast<std::int64_t>(actual_left);
    energy += static_cast<std::uint64_t>(wide_left * wide_left);
  }
  CHECK(energy == 800ULL * static_cast<std::uint64_t>(amplitude) *
                      static_cast<std::uint64_t>(amplitude));
}

} // namespace

TEST_CASE("synthetic content has a bounded versioned identity") {
  const auto parsed = tessera::parse_synthetic_content(content(2, 7, 1));
  CHECK(parsed.profile == 2);
  CHECK(parsed.initial_value == 7);
  CHECK(parsed.flags == 1);
  CHECK(tessera::parse_synthetic_content(content(1, 0)).initial_value == 0);
  CHECK(tessera::parse_synthetic_content(
            content(1, std::numeric_limits<std::uint32_t>::max()))
            .initial_value == std::numeric_limits<std::uint32_t>::max());
  for (const auto profile :
       {0U, 3U, std::numeric_limits<std::uint32_t>::max()}) {
    CHECK_THROWS((void)tessera::parse_synthetic_content(content(profile)));
  }
  CHECK_THROWS((void)tessera::parse_synthetic_content(content(1, 7, 2)));
  CHECK_THROWS((void)tessera::parse_synthetic_content(content(1, 7, 0, 1)));
  auto damaged = content(1);
  damaged[0] ^= 1;
  CHECK_THROWS((void)tessera::parse_synthetic_content(damaged));
  for (std::size_t count = 0; count < 24; ++count) {
    const auto complete = content(1);
    CHECK_THROWS((void)tessera::parse_synthetic_content(
        std::span(complete).first(count)));
  }
  damaged = content(1);
  damaged.push_back(0);
  CHECK_THROWS((void)tessera::parse_synthetic_content(damaged));
}

TEST_CASE("synthetic profiles expose only implemented capabilities") {
  auto minimal =
      tessera::make_synthetic_backend(minimal_id, content(1), initial_memory);
  auto complete =
      tessera::make_synthetic_backend(complete_id, content(2), initial_memory);
  REQUIRE(minimal);
  REQUIRE(complete);
  CHECK(minimal->backend_id() == minimal_id);
  CHECK(complete->backend_id() == complete_id);
  CHECK(minimal->capabilities() == 0x120FULL);
  CHECK(complete->capabilities() == 0x1FFFULL);
  CHECK((complete->capabilities() & (1ULL << 9)) != 0);
  CHECK(minimal->frame() == 0);
  CHECK(minimal->tick() == 0);
  CHECK(minimal->latched_inputs() == InputState{});
  CHECK(minimal->pixels().empty());
  CHECK(minimal->pcm().empty());
  const auto initial = inspect_machine(minimal->machine_record());
  CHECK(initial.accumulator == 7);
  CHECK(initial.memory == initial_memory);
  CHECK(initial.phase == 7);
  CHECK_THROWS((void)tessera::make_synthetic_backend(minimal_id, content(2),
                                                     initial_memory));
  CHECK_THROWS((void)tessera::make_synthetic_backend(complete_id, content(1),
                                                     initial_memory));
  CHECK_THROWS((void)tessera::make_synthetic_backend("unknown", content(1),
                                                     initial_memory));
  CHECK_THROWS((void)tessera::make_synthetic_backend(
      minimal_id, content(1), std::span(initial_memory).first(15)));
  auto oversized = Bytes(initial_memory.begin(), initial_memory.end());
  oversized.push_back(0);
  CHECK_THROWS(
      (void)tessera::make_synthetic_backend(minimal_id, content(1), oversized));
}

TEST_CASE("synthetic frame one has independent neutral and action goldens") {
  for (const auto mask : {0U, 32U}) {
    CAPTURE(mask);
    auto backend = tessera::make_synthetic_backend(complete_id, content(2),
                                                   initial_memory);
    backend->step(inputs(mask));
    PrefixOracle oracle;
    oracle.record(mask);
    check_outputs(*backend, oracle, mask, 7);
    const auto state = inspect_machine(backend->machine_record());
    CHECK(state.accumulator == (mask == 0 ? 8U : 40U));
    CHECK(state.memory[0] == 4);
    CHECK(state.phase == 39);
    const std::array<std::uint8_t, 4> center =
        mask == 0 ? std::array<std::uint8_t, 4>{8, 16, 5, 255}
                  : std::array<std::uint8_t, 4>{40, 48, 5, 255};
    CHECK(std::equal(center.begin(), center.end(),
                     backend->pixels().begin() + (8 + 16 * 8) * 4));
  }
}

TEST_CASE("every digital input and persistent audio phase affect the declared "
          "model") {
  for (unsigned bit = 0; bit < 8; ++bit) {
    CAPTURE(bit);
    auto backend = tessera::make_synthetic_backend(complete_id, content(2),
                                                   initial_memory);
    PrefixOracle oracle;
    const auto mask = 1U << bit;
    backend->step(inputs(mask));
    oracle.record(mask);
    check_outputs(*backend, oracle, mask, 7);
  }
  auto backend =
      tessera::make_synthetic_backend(complete_id, content(2), initial_memory);
  PrefixOracle oracle;
  for (std::uint32_t frame = 1; frame <= 32; ++frame) {
    const auto mask = frame == 32 ? 255U : (frame * 73) % 256;
    const auto previous_phase = oracle.state().phase;
    backend->step(inputs(mask));
    oracle.record(mask);
    check_outputs(*backend, oracle, mask, previous_phase);
  }
}

TEST_CASE("profiles and external reset fault flags do not change the machine "
          "formulas") {
  auto minimal =
      tessera::make_synthetic_backend(minimal_id, content(1), initial_memory);
  auto complete =
      tessera::make_synthetic_backend(complete_id, content(2), initial_memory);
  auto fault = tessera::make_synthetic_backend(complete_id, content(2, 7, 1),
                                               initial_memory);
  for (std::uint32_t frame = 1; frame <= 60; ++frame) {
    const auto input = inputs(frame >= 21 && frame <= 40 ? 32U : 0U);
    minimal->step(input);
    complete->step(input);
    fault->step(input);
    CHECK(minimal->machine_record() == complete->machine_record());
    CHECK(minimal->pixels() == complete->pixels());
    CHECK(minimal->pcm() == complete->pcm());
    CHECK(fault->machine_record() == complete->machine_record());
    CHECK(fault->pixels() == complete->pixels());
    CHECK(fault->pcm() == complete->pcm());
  }
  CHECK_THROWS((void)minimal->regions(1));
}

TEST_CASE(
    "synthetic ownership and invalid inputs preserve the completed frame") {
  auto memory = Bytes(initial_memory.begin(), initial_memory.end());
  auto fixture = content(2);
  std::string id{complete_id};
  auto backend = tessera::make_synthetic_backend(id, fixture, memory);
  std::fill(memory.begin(), memory.end(), 0);
  std::fill(fixture.begin(), fixture.end(), 0);
  std::fill(id.begin(), id.end(), 'x');
  backend->step(inputs(32));
  CHECK(backend->backend_id() == complete_id);
  CHECK(inspect_machine(backend->machine_record()).memory[5] == 90);
  const auto state = backend->machine_record();
  const auto pixels = backend->pixels();
  const auto pcm = backend->pcm();
  const auto latched = backend->latched_inputs();
  for (const auto value : {-1, 2, std::numeric_limits<std::int32_t>::max()}) {
    auto invalid = inputs(0);
    invalid[7] = value;
    CHECK_THROWS(backend->step(invalid));
    CHECK(backend->frame() == 1);
    CHECK(backend->tick() == 1000);
    CHECK(backend->machine_record() == state);
    CHECK(backend->pixels() == pixels);
    CHECK(backend->pcm() == pcm);
    CHECK(backend->latched_inputs() == latched);
  }
  auto wrapping = tessera::make_synthetic_backend(
      minimal_id, content(1, std::numeric_limits<std::uint32_t>::max()),
      initial_memory);
  wrapping->step(inputs(0));
  CHECK(inspect_machine(wrapping->machine_record()).accumulator == 0);
  CHECK(inspect_machine(wrapping->machine_record()).phase == 31);
}

TEST_CASE("synthetic complete regions agree with machine and original video") {
  auto backend =
      tessera::make_synthetic_backend(complete_id, content(2), initial_memory);
  backend->step(inputs(32));
  const auto before = backend->machine_record();
  const auto regions = backend->regions(31);
  REQUIRE(regions.size() == 5);
  for (std::size_t index = 0; index < regions.size(); ++index) {
    CHECK(regions[index].kind == index + 1);
  }
  CHECK(regions[0].encoding == 5);
  CHECK(regions[0].element_bytes == 1);
  CHECK(regions[0].element_count == 16);
  const auto machine = inspect_machine(before);
  CHECK(regions[0].data == Bytes(machine.memory.begin(), machine.memory.end()));
  CHECK(regions[1].encoding == 5);
  CHECK(regions[1].element_bytes == 4);
  CHECK(regions[1].element_count == 256);
  REQUIRE(regions[1].data.size() == 1024);
  for (std::uint32_t index = 0; index < 256; ++index) {
    const auto expected = palette_colour(index, 32, 1, machine);
    CHECK(std::equal(expected.begin(), expected.end(),
                     regions[1].data.begin() + index * 4));
  }
  CHECK(regions[2].encoding == 5);
  REQUIRE(regions[2].data.size() == 8);
  CHECK(read_le(regions[2].data, 0, 2) == 0);
  CHECK(read_le(regions[2].data, 2, 2) == 4);
  CHECK(read_le(regions[2].data, 4, 2) == 255);
  CHECK(read_le(regions[2].data, 6, 2) == 1);
  CHECK(regions[3].encoding == 11);
  REQUIRE(regions[3].data.size() == 80);
  for (std::size_t field = 0; field < 4; ++field) {
    CHECK(read_le(regions[3].data, field * 20, 4) == field + 1);
    CHECK(read_le(regions[3].data, field * 20 + 4, 4) == 1);
    CHECK(read_le(regions[3].data, field * 20 + 8, 8) == 4);
    CHECK(read_le(regions[3].data, field * 20 + 16, 4) == (field < 2 ? 16 : 1));
  }
  CHECK(regions[4].encoding == 11);
  REQUIRE(regions[4].data.size() == 89);
  for (std::size_t field = 0; field < 3; ++field) {
    CHECK(read_le(regions[4].data, field * 24, 4) == field + 1);
    CHECK(read_le(regions[4].data, field * 24 + 4, 4) == 3);
    CHECK(read_le(regions[4].data, field * 24 + 8, 8) == 8);
  }
  CHECK(read_le(regions[4].data, 16, 8) == 1);
  CHECK(read_le(regions[4].data, 40, 8) == 0);
  CHECK(read_le(regions[4].data, 64, 8) == 1000);
  CHECK(read_le(regions[4].data, 72, 4) == 4);
  CHECK(read_le(regions[4].data, 76, 4) == 7);
  CHECK(read_le(regions[4].data, 80, 8) == 1);
  CHECK(regions[4].data[88] == 1);
  const auto memory_only = backend->regions(1);
  REQUIRE(memory_only.size() == 1);
  CHECK(memory_only.front().kind == 1);
  CHECK(backend->regions(0).empty());
  CHECK_THROWS((void)backend->regions(32));
  CHECK_THROWS((void)backend->regions(33));
  CHECK(backend->machine_record() == before);
}
