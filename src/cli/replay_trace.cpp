#include "replay_trace.hpp"

#include <array>
#include <initializer_list>
#include <limits>
#include <set>
#include <utility>

namespace tessera::cli {
namespace {
void valid(bool condition) {
  ensure(condition, TESSERA_INVALID_TRACE, "La traza no es válida.");
}
void keys(const Json &value, std::initializer_list<std::string_view> expected) {
  valid(value.is_object() && value.size() == expected.size());
  for (const auto key : expected)
    valid(value.contains(std::string(key)));
}
std::uint64_t unsigned_number(const Json &value, std::uint64_t maximum) {
  valid(value.is_number_unsigned());
  const auto result = value.get<std::uint64_t>();
  valid(result <= maximum);
  return result;
}
Bytes unhex(const Json &value) {
  valid(value.is_string());
  const auto text = value.get<std::string>();
  valid(text.size() == 64);
  Bytes out;
  const auto digit = [](char ch) -> unsigned {
    if (ch >= '0' && ch <= '9')
      return static_cast<unsigned>(ch - '0');
    if (ch >= 'a' && ch <= 'f')
      return static_cast<unsigned>(ch - 'a') + 10;
    if (ch >= 'A' && ch <= 'F')
      return static_cast<unsigned>(ch - 'A') + 10;
    valid(false);
    return 0;
  };
  for (std::size_t index = 0; index < text.size(); index += 2)
    out.push_back(static_cast<std::uint8_t>(digit(text[index]) * 16 +
                                            digit(text[index + 1])));
  return out;
}
Json strict_json(View input) {
  std::vector<std::set<std::string>> objects;
  const auto callback = [&objects](int depth, Json::parse_event_t event,
                                   Json &value) {
    valid(depth <= 32);
    if (event == Json::parse_event_t::object_start)
      objects.emplace_back();
    if (event == Json::parse_event_t::key)
      valid(!objects.empty() &&
            objects.back().insert(value.get<std::string>()).second);
    if (event == Json::parse_event_t::object_end) {
      valid(!objects.empty());
      objects.pop_back();
    }
    return true;
  };
  auto result = Json::parse(input.begin(), input.end(), callback, false);
  valid(!result.is_discarded());
  return result;
}
Bytes encode_json(const Json &root) {
  keys(root, {"trace_version", "content_sha256", "config_sha256", "length",
              "frames"});
  valid(unsigned_number(root.at("trace_version"), 1) == 1);
  const auto count = unsigned_number(root.at("length"), 65536);
  valid(count != 0 && root.at("frames").is_array() &&
        root.at("frames").size() == count);
  Bytes out = text_bytes("TSTRACE1");
  field(out, 1, 5, unhex(root.at("content_sha256")));
  field(out, 2, 5, unhex(root.at("config_sha256")));
  scalar(out, 3, 3, count);
  std::vector<Bytes> frames;
  frames.reserve(static_cast<std::size_t>(count));
  std::uint64_t next = 1;
  for (const auto &source : root.at("frames")) {
    keys(source, {"frame", "assignments"});
    valid(unsigned_number(source.at("frame"), 65536) == next);
    const auto &assignments = source.at("assignments");
    valid(assignments.is_array() && !assignments.empty() &&
          assignments.size() <= 64);
    std::vector<Bytes> controls;
    std::pair<std::uint64_t, std::uint64_t> previous{};
    bool first = true;
    for (const auto &assignment : assignments) {
      keys(assignment, {"port", "control", "value"});
      const auto port = unsigned_number(
          assignment.at("port"), std::numeric_limits<std::uint32_t>::max());
      const auto control = unsigned_number(
          assignment.at("control"), std::numeric_limits<std::uint32_t>::max());
      const auto pair = std::pair{port, control};
      valid(first || previous < pair);
      previous = pair;
      first = false;
      const auto &raw = assignment.at("value");
      valid(raw.is_number_integer());
      if (raw.is_number_unsigned())
        valid(raw.get<std::uint64_t>() <=
              std::numeric_limits<std::int32_t>::max());
      const auto value = raw.get<std::int64_t>();
      valid(value >= std::numeric_limits<std::int32_t>::min() &&
            value <= std::numeric_limits<std::int32_t>::max());
      Bytes item;
      scalar(item, 1, 1, port);
      scalar(item, 2, 1, control);
      scalar(item, 3, 2,
             static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
      controls.push_back(std::move(item));
    }
    Bytes frame;
    scalar(frame, 1, 3, next++);
    field(frame, 2, 8, array(controls));
    frames.push_back(std::move(frame));
  }
  field(out, 4, 8, array(frames));
  valid(out.size() <= file_limit);
  return out;
}
std::uint64_t trace_length(View bytes) {
  std::size_t offset = 8;
  while (offset < bytes.size()) {
    const auto tag = read_le(bytes, offset, 4);
    const auto length = read_le(bytes, offset + 8, 8);
    valid(offset + 16 <= bytes.size() && length <= bytes.size() - offset - 16);
    if (tag == 3) {
      valid(length == 8);
      return read_le(bytes, offset + 16, 8);
    }
    offset += 16 + static_cast<std::size_t>(length);
  }
  valid(false);
  return 0;
}
} // namespace

ReplayTrace parse_trace(View input) {
  valid(!input.empty() && input.size() <= file_limit);
  ReplayTrace result;
  constexpr std::array<std::uint8_t, 8> magic{'T', 'S', 'T', 'R',
                                              'A', 'C', 'E', '1'};
  bool invalid_json = false;
  try {
    if (input.size() >= magic.size() &&
        std::equal(magic.begin(), magic.end(), input.begin()))
      result.bytes.assign(input.begin(), input.end());
    else {
      result.json = strict_json(input);
      result.bytes = encode_json(result.json);
    }
  } catch (const Json::exception &) {
    invalid_json = true;
  }
  if (invalid_json) {
    fail(TESSERA_INVALID_TRACE, TESSERA_PHASE_VALIDATE,
         "El JSON de la traza no es válido.");
  }
  auto context = init<tessera_context_v1>();
  require_ok(tessera_trace_validate_v1(view(result.bytes), &context), context);
  result.length = trace_length(result.bytes);
  return result;
}

ReplayTrace make_demo_trace(View content, View config, std::uint64_t frames,
                            std::string_view scenario) {
  ensure(frames > 0 && frames <= 65536 &&
         (scenario == "neutral" || scenario == "action"));
  Json source{{"trace_version", 1U},
              {"content_sha256", digest(content)},
              {"config_sha256", digest(config)},
              {"length", frames},
              {"frames", Json::array()}};
  const auto first = frames <= 60 ? 21ULL : 301ULL;
  const auto last = frames <= 60 ? 40ULL : 600ULL;
  for (std::uint64_t frame = 1; frame <= frames; ++frame) {
    Json assignments = Json::array();
    for (const auto item : digital_inputs(
             scenario == "action" && frame >= first && frame <= last ? 32U
                                                                     : 0U))
      assignments.push_back({{"port", item.port},
                             {"control", item.control},
                             {"value", item.value}});
    source["frames"].push_back(
        {{"frame", frame}, {"assignments", std::move(assignments)}});
  }
  const auto encoded = source.dump();
  return parse_trace(View{
      reinterpret_cast<const std::uint8_t *>(encoded.data()), encoded.size()});
}
} // namespace tessera::cli
