#pragma once
#include "domain/result.hpp"
#include "systems/emulated_system.hpp"
#include <limits>
#include <picosha2.h>
#include <span>
#include <string_view>

namespace tessera::wire {
using View = std::span<const std::uint8_t>;
inline Bytes text(std::string_view s) { return {s.begin(), s.end()}; }
inline void append(Bytes &b, View v) { b.insert(b.end(), v.begin(), v.end()); }
inline void put(Bytes &b, std::uint64_t v, unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
inline Bytes number(std::uint64_t v, unsigned n) {
  Bytes b;
  put(b, v, n);
  return b;
}
inline void field(Bytes &b, std::uint32_t tag, std::uint32_t type, View v) {
  put(b, tag, 4);
  put(b, type, 4);
  put(b, v.size(), 8);
  append(b, v);
}
inline void scalar(Bytes &b, std::uint32_t tag, std::uint32_t type,
                   std::uint64_t n) {
  field(b, tag, type, number(n, type <= 2 ? 4 : 8));
}
inline Bytes array(const std::vector<Bytes> &items) {
  Bytes b;
  put(b, items.size(), 8);
  for (const auto &item : items) {
    put(b, item.size(), 8);
    append(b, item);
  }
  return b;
}
inline Bytes sha(View v) {
  Bytes b(32);
  picosha2::hash256(v.begin(), v.end(), b.begin(), b.end());
  return b;
}
inline std::uint64_t read(View b, std::size_t at, unsigned n) {
  require(at <= b.size() && n <= b.size() - at, TESSERA_INVALID_TRACE);
  std::uint64_t v{};
  for (unsigned i = 0; i < n; ++i)
    v |= std::uint64_t(b[at + i]) << (8 * i);
  return v;
}
struct Field {
  std::uint32_t tag, type;
  View data;
};
inline std::vector<Field> record(View b,
                                 tessera_status invalid = TESSERA_INVALID_TRACE,
                                 std::uint32_t phase = 0) {
  std::vector<Field> out;
  std::uint32_t previous{};
  while (!b.empty()) {
    require(b.size() >= 16, invalid, phase);
    const auto tag = static_cast<std::uint32_t>(read(b, 0, 4));
    const auto type = static_cast<std::uint32_t>(read(b, 4, 4));
    const auto n = read(b, 8, 8);
    require(tag > previous && type >= 1 && type <= 11 && n <= b.size() - 16,
            invalid, phase);
    out.push_back({tag, type, b.subspan(16, static_cast<std::size_t>(n))});
    previous = tag;
    b = b.subspan(16 + static_cast<std::size_t>(n));
  }
  return out;
}
inline View get(const std::vector<Field> &f, std::size_t index,
                std::uint32_t tag, std::uint32_t type,
                std::size_t bytes = std::numeric_limits<std::size_t>::max(),
                tessera_status invalid = TESSERA_INVALID_TRACE,
                std::uint32_t phase = 0) {
  require(index < f.size() && f[index].tag == tag && f[index].type == type,
          invalid, phase);
  require(bytes == std::numeric_limits<std::size_t>::max() ||
              f[index].data.size() == bytes,
          invalid, phase);
  return f[index].data;
}
inline std::vector<View> elements(View b, std::uint64_t maximum) {
  const auto n = read(b, 0, 8);
  require(n <= maximum, TESSERA_INVALID_TRACE);
  b = b.subspan(8);
  std::vector<View> v;
  v.reserve(static_cast<std::size_t>(n));
  for (std::uint64_t i = 0; i < n; ++i) {
    const auto size = read(b, 0, 8);
    b = b.subspan(8);
    require(size <= b.size(), TESSERA_INVALID_TRACE);
    v.push_back(b.first(static_cast<std::size_t>(size)));
    b = b.subspan(static_cast<std::size_t>(size));
  }
  require(b.empty(), TESSERA_INVALID_TRACE);
  return v;
}
inline Bytes inputs(const InputState &input) {
  std::vector<Bytes> entries;
  for (std::uint32_t i = 0; i < 8; ++i) {
    Bytes e;
    scalar(e, 1, 1, 0);
    scalar(e, 2, 1, i);
    scalar(e, 3, 2, static_cast<std::uint32_t>(input[i]));
    entries.push_back(std::move(e));
  }
  return array(entries);
}
inline Bytes configuration(std::string_view id, View build, View memory) {
  Bytes b;
  const bool megadrive = id == "clownmdemu";
  scalar(b, 1, 1, megadrive ? TESSERA_SYSTEM_MEGA_DRIVE : TESSERA_SYSTEM_NONE);
  field(b, 2, 6, text(id));
  field(b, 3, 5, build);
  scalar(b, 4, 1, megadrive ? 1 : 0);
  scalar(b, 5, 1, 48000);
  std::vector<Bytes> map;
  for (std::uint32_t i = 0; i < 8; ++i) {
    Bytes v;
    scalar(v, 1, 1, 0);
    scalar(v, 2, 1, i);
    scalar(v, 3, 1, 1);
    scalar(v, 4, 2, 0);
    scalar(v, 5, 2, 1);
    map.push_back(std::move(v));
  }
  field(b, 6, 8, array(map));
  field(b, 7, 5, sha(memory));
  if (megadrive) {
    scalar(b, 8, 1, 895780);
    scalar(b, 9, 1, 53693175);
    scalar(b, 10, 1, 1);
    scalar(b, 11, 2, 32768);
    scalar(b, 12, 2, 4096);
    field(b, 13, 6, text("integer-causal-ZOH-v1;CD-disabled-zero-required;zero-initialized-upstream-options"));
  }
  return b;
}
struct Trace {
  Bytes content_sha, config_sha;
  std::vector<InputState> frames;
};
inline Trace trace(View b) {
  require(b.size() >= 8 &&
              std::equal(b.begin(), b.begin() + 8, text("TSTRACE1").begin()),
          TESSERA_INVALID_TRACE);
  const auto f = record(b.subspan(8));
  require(f.size() == 4, TESSERA_INVALID_TRACE);
  Trace t;
  const auto content = get(f, 0, 1, 5, 32), config = get(f, 1, 2, 5, 32);
  t.content_sha.assign(content.begin(), content.end());
  t.config_sha.assign(config.begin(), config.end());
  const auto n = read(get(f, 2, 3, 3, 8), 0, 8);
  require(n > 0 && n <= 65536, TESSERA_INVALID_TRACE);
  const auto frames = elements(get(f, 3, 4, 8), 65536);
  require(frames.size() == n, TESSERA_INVALID_TRACE);
  t.frames.reserve(frames.size());
  for (std::size_t i = 0; i < frames.size(); ++i) {
    const auto r = record(frames[i]);
    require(r.size() == 2 && read(get(r, 0, 1, 3, 8), 0, 8) == i + 1,
            TESSERA_INVALID_TRACE);
    const auto assignments = elements(get(r, 1, 2, 8), 64);
    require(assignments.size() == 8, TESSERA_INVALID_TRACE);
    InputState state{};
    for (std::size_t k = 0; k < 8; ++k) {
      const auto e = record(assignments[k]);
      require(e.size() == 3 && read(get(e, 0, 1, 1, 4), 0, 4) == 0 &&
                  read(get(e, 1, 2, 1, 4), 0, 4) == k,
              TESSERA_INVALID_TRACE);
      const auto v = read(get(e, 2, 3, 2, 4), 0, 4);
      require(v <= 1, TESSERA_INVALID_TRACE);
      state[k] = static_cast<std::int32_t>(v);
    }
    t.frames.push_back(state);
  }
  return t;
}
} // namespace tessera::wire
