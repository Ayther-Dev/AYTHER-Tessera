#include "persistence/state_format.hpp"

#include "domain/result.hpp"
#include "domain/wire.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

namespace tessera::persistence {
namespace {
constexpr std::size_t header_bytes = 64;
constexpr std::size_t maximum_bytes = 64 * 1024 * 1024;
using wire::View;

bool equal(View left, View right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin());
}

void valid(bool condition) {
  require(condition, TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
}

std::uint64_t read_integer(View bytes, std::size_t offset, unsigned width) {
  valid(width <= 8 && offset <= bytes.size() &&
        width <= bytes.size() - offset);
  std::uint64_t value{};
  for (unsigned index = 0; index < width; ++index)
    value |= std::uint64_t{bytes[offset + index]} << (index * 8);
  return value;
}

std::vector<wire::Field> record(View bytes) {
  std::vector<wire::Field> fields;
  std::uint32_t previous{};
  while (!bytes.empty()) {
    valid(bytes.size() >= 16);
    const auto tag = static_cast<std::uint32_t>(read_integer(bytes, 0, 4));
    const auto type = static_cast<std::uint32_t>(read_integer(bytes, 4, 4));
    const auto length = read_integer(bytes, 8, 8);
    valid(tag > previous && type >= 1 && type <= 11 &&
          length <= bytes.size() - 16);
    fields.push_back({tag, type, bytes.subspan(16,
                         static_cast<std::size_t>(length))});
    previous = tag;
    bytes = bytes.subspan(16 + static_cast<std::size_t>(length));
  }
  return fields;
}

View field(const std::vector<wire::Field> &fields, std::size_t index,
           std::uint32_t tag, std::uint32_t encoding,
           std::size_t size = std::numeric_limits<std::size_t>::max()) {
  valid(index < fields.size());
  const auto &selected = fields[index];
  valid(selected.tag == tag && selected.type == encoding &&
        (size == std::numeric_limits<std::size_t>::max() ||
         selected.data.size() == size));
  return selected.data;
}

std::vector<View> elements(View bytes, std::uint64_t maximum) {
  const auto count = read_integer(bytes, 0, 8);
  valid(count <= maximum);
  bytes = bytes.subspan(8);
  std::vector<View> values;
  values.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    const auto size = read_integer(bytes, 0, 8);
    bytes = bytes.subspan(8);
    valid(size <= bytes.size());
    values.push_back(bytes.first(static_cast<std::size_t>(size)));
    bytes = bytes.subspan(static_cast<std::size_t>(size));
  }
  valid(bytes.empty());
  return values;
}

std::uint64_t integer(const std::vector<wire::Field> &fields,
                      std::size_t index, std::uint32_t tag,
                      std::uint32_t encoding, std::size_t bytes) {
  return read_integer(field(fields, index, tag, encoding, bytes), 0,
                      static_cast<unsigned>(bytes));
}

InputState parse_inputs(View bytes) {
  InputState result{};
  const auto entries = elements(bytes, 64);
  valid(entries.size() == result.size());
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto fields = record(entries[index]);
    valid(fields.size() == 3 &&
          integer(fields, 0, 1, TESSERA_ENCODING_U32, 4) == 0 &&
          integer(fields, 1, 2, TESSERA_ENCODING_U32, 4) == index);
    const auto value = integer(fields, 2, 3, TESSERA_ENCODING_I32, 4);
    valid(value <= 1);
    result[index] = static_cast<std::int32_t>(value);
  }
  return result;
}
} // namespace

Bytes encode_checkpoint(View state, View trace, std::uint64_t next,
                        View initial_sha) {
  require(initial_sha.size() == 32 && !state.empty() &&
              state.size() <= maximum_bytes - 512,
          TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_WRITE);
  const auto parsed_trace = trace.empty() ? std::optional<wire::Trace>{}
                                          : std::optional<wire::Trace>{wire::trace(trace)};
  require((parsed_trace && next > 0 &&
           next <= parsed_trace->frames.size() + 1) ||
              (!parsed_trace && next == 0),
          TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_WRITE);
  Bytes payload;
  wire::field(payload, 1, TESSERA_ENCODING_BYTES, state);
  wire::field(payload, 2, TESSERA_ENCODING_BYTES,
              parsed_trace ? wire::sha(trace) : Bytes(32, 0));
  wire::scalar(payload, 3, TESSERA_ENCODING_U64,
               parsed_trace ? parsed_trace->frames.size() : 0);
  wire::scalar(payload, 4, TESSERA_ENCODING_U64, next);
  wire::field(payload, 5, TESSERA_ENCODING_BYTES, initial_sha);
  require(payload.size() <= maximum_bytes - header_bytes,
          TESSERA_BUDGET_EXCEEDED, TESSERA_PHASE_WRITE);
  Bytes encoded = wire::text("TSCHK001");
  wire::put(encoded, 1, 2);
  wire::put(encoded, 0, 2);
  wire::put(encoded, header_bytes, 4);
  wire::put(encoded, header_bytes + payload.size(), 8);
  wire::append(encoded, wire::sha(payload));
  wire::put(encoded, 5, 4);
  wire::put(encoded, 0, 4);
  wire::append(encoded, payload);
  return encoded;
}

CheckpointData decode_checkpoint(View bytes) {
  valid(bytes.size() >= header_bytes && bytes.size() <= maximum_bytes);
  valid(equal(bytes.first(8), wire::text("TSCHK001")));
  valid(read_integer(bytes, 8, 2) == 1 &&
        read_integer(bytes, 10, 2) == 0 &&
        read_integer(bytes, 12, 4) == header_bytes &&
        read_integer(bytes, 16, 8) == bytes.size() &&
        read_integer(bytes, 56, 4) == 5 &&
        read_integer(bytes, 60, 4) == 0);
  valid(equal(bytes.subspan(24, 32), wire::sha(bytes.subspan(header_bytes))));
  const auto fields = record(bytes.subspan(header_bytes));
  valid(fields.size() == 5);
  CheckpointData result;
  const auto state = field(fields, 0, 1, TESSERA_ENCODING_BYTES);
  valid(state.size() >= 8 && state.size() <= maximum_bytes);
  result.state.assign(state.begin(), state.end());
  const auto trace_sha = field(fields, 1, 2, TESSERA_ENCODING_BYTES, 32);
  result.trace_sha.assign(trace_sha.begin(), trace_sha.end());
  result.trace_length = integer(fields, 2, 3, TESSERA_ENCODING_U64, 8);
  result.next_replay_frame = integer(fields, 3, 4, TESSERA_ENCODING_U64, 8);
  const auto initial = field(fields, 4, 5, TESSERA_ENCODING_BYTES, 32);
  result.initial_sha.assign(initial.begin(), initial.end());
  valid((result.trace_length == 0 && result.next_replay_frame == 0 &&
         std::all_of(result.trace_sha.begin(), result.trace_sha.end(),
                     [](auto byte) { return byte == 0; })) ||
        (result.trace_length > 0 && result.trace_length <= 65536 &&
         result.next_replay_frame > 0 &&
         result.next_replay_frame <= result.trace_length + 1));
  return result;
}

ParsedState decode_state(View bytes, std::string_view backend,
                         View content_sha, View build_id, View config_sha,
                         std::uint64_t ticks_per_frame) {
  valid(bytes.size() >= 8 && equal(bytes.first(8), wire::text("TSSTATE1")));
  const auto fields = record(bytes.subspan(8));
  valid(fields.size() == 10);
  const auto codec = field(fields, 0, 1, TESSERA_ENCODING_UTF8);
  const auto expected = wire::text(backend == "clownmdemu"
                                       ? "tessera.megadrive.clown"
                                       : "tessera.synthetic");
  require(equal(codec, expected) &&
              integer(fields, 1, 2, TESSERA_ENCODING_U32, 4) == 1 &&
              integer(fields, 2, 3, TESSERA_ENCODING_U32, 4) == 0,
          TESSERA_CHECKPOINT_INCOMPATIBLE, TESSERA_PHASE_RESTORE);
  require(equal(field(fields, 3, 4, TESSERA_ENCODING_BYTES, 32),
                content_sha) &&
              equal(field(fields, 4, 5, TESSERA_ENCODING_BYTES, 32),
                    build_id) &&
              equal(field(fields, 5, 6, TESSERA_ENCODING_BYTES, 32),
                    config_sha),
          TESSERA_CHECKPOINT_INCOMPATIBLE, TESSERA_PHASE_RESTORE);
  ParsedState result;
  result.completed_frames = integer(fields, 6, 7, TESSERA_ENCODING_U64, 8);
  result.logical_tick = integer(fields, 7, 8, TESSERA_ENCODING_U64, 8);
  valid(ticks_per_frame > 0 &&
        result.completed_frames <=
            std::numeric_limits<std::uint64_t>::max() / ticks_per_frame &&
        result.logical_tick == result.completed_frames * ticks_per_frame);
  result.inputs =
      parse_inputs(field(fields, 8, 9, TESSERA_ENCODING_ARRAY));
  const auto machine = field(fields, 9, 65536, TESSERA_ENCODING_RECORD);
  result.machine.assign(machine.begin(), machine.end());
  return result;
}

} // namespace tessera::persistence
