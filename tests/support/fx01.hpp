#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <picosha2.h>
#include <span>
#include <stdexcept>
#include <string>
#include <tessera/tessera.h>
#include <vector>

namespace fx01 {
using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;
template <class T> T init() {
  T v{};
  v.header = {sizeof(T), 1, 0};
  return v;
}
inline void expect(tessera_status s) {
  if (s != TESSERA_OK)
    throw std::runtime_error("PUBLIC_API_FAILED:" + std::to_string(s));
}
inline void append(Bytes &b, View v) { b.insert(b.end(), v.begin(), v.end()); }
inline Bytes str(std::string_view s) { return {s.begin(), s.end()}; }
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
  for (auto &v : items) {
    put(b, v.size(), 8);
    append(b, v);
  }
  return b;
}
inline std::uint64_t read(View b, std::size_t at, unsigned n) {
  if (at > b.size() || n > b.size() - at)
    throw std::runtime_error("TEST_WIRE_TRUNCATED");
  std::uint64_t v{};
  for (unsigned i = 0; i < n; ++i)
    v |= std::uint64_t(b[at + i]) << (8 * i);
  return v;
}
inline Bytes sha(View v) {
  Bytes b(32);
  picosha2::hash256(v.begin(), v.end(), b.begin(), b.end());
  return b;
}
inline std::string hex(View v) {
  return picosha2::hash256_hex_string(v.begin(), v.end());
}
inline Bytes memory() {
  Bytes b;
  for (unsigned j = 0; j < 16; ++j)
    b.push_back(static_cast<std::uint8_t>(3 + 17 * j));
  return b;
}
inline Bytes content(unsigned profile, unsigned flags = 0,
                     std::uint32_t initial = 7) {
  Bytes b = str("TSSYN001");
  for (auto n : {profile, initial, flags, 0U})
    put(b, n, 4);
  return b;
}
inline tessera_backend_info_v1 backend(bool complete = true) {
  auto c = init<tessera_context_v1>();
  std::uint32_t required{};
  std::array<tessera_backend_info_v1, 3> out{init<tessera_backend_info_v1>(),
                                             init<tessera_backend_info_v1>(),
                                             init<tessera_backend_info_v1>()};
  expect(tessera_backends_enumerate_v1(out.data(), 3, &required, &c));
  for (auto b : out)
    if (std::string(b.id) ==
        (complete ? "synthetic.complete" : "synthetic.minimal"))
      return b;
  throw std::runtime_error("TEST_BACKEND_MISSING");
}
inline Bytes config(const tessera_backend_info_v1 &b, View pm) {
  Bytes result;
  scalar(result, 1, 1, 0);
  field(result, 2, 6, str(b.id));
  field(result, 3, 5, b.build_id);
  scalar(result, 4, 1, 0);
  scalar(result, 5, 1, 48000);
  std::vector<Bytes> controls;
  for (unsigned i = 0; i < 8; ++i) {
    Bytes v;
    scalar(v, 1, 1, 0);
    scalar(v, 2, 1, i);
    scalar(v, 3, 1, 1);
    scalar(v, 4, 2, 0);
    scalar(v, 5, 2, 1);
    controls.push_back(v);
  }
  field(result, 6, 8, array(controls));
  field(result, 7, 5, sha(pm));
  return result;
}
inline tessera_bytes_v1 view(View b) { return {b.data(), b.size()}; }
inline std::array<tessera_input_assignment_v1, 8> inputs(unsigned mask = 0) {
  std::array<tessera_input_assignment_v1, 8> a{};
  for (std::uint16_t i = 0; i < 8; ++i)
    a[i] = {0, i, static_cast<std::int32_t>((mask >> i) & 1)};
  return a;
}
inline Bytes input_wire(unsigned mask) {
  std::vector<Bytes> items;
  for (auto a : inputs(mask)) {
    Bytes e;
    scalar(e, 1, 1, a.port);
    scalar(e, 2, 1, a.control);
    scalar(e, 3, 2, a.value);
    items.push_back(e);
  }
  return array(items);
}
inline Bytes trace(View rom, View cfg, std::span<const unsigned> masks) {
  Bytes b = str("TSTRACE1");
  field(b, 1, 5, sha(rom));
  field(b, 2, 5, sha(cfg));
  scalar(b, 3, 3, masks.size());
  std::vector<Bytes> frames;
  for (std::size_t i = 0; i < masks.size(); ++i) {
    Bytes f;
    scalar(f, 1, 3, i + 1);
    field(f, 2, 8, input_wire(masks[i]));
    frames.push_back(f);
  }
  field(b, 4, 8, array(frames));
  return b;
}
struct Session {
  tessera_session handle{};
  tessera_context_v1 context = init<tessera_context_v1>();
  Bytes rom, pm, cfg;
  std::string id;
  explicit Session(bool complete = true, unsigned flags = 0)
      : rom(content(complete ? 2 : 1, flags)), pm(memory()) {
    const auto b = backend(complete);
    id = b.id;
    cfg = config(b, pm);
    auto creation = init<tessera_session_create_info_v1>();
    // Keep the ID bytes alive until create returns.
    const auto idBytes = str(id);
    creation.backend_id = view(idBytes);
    creation.content = view(rom);
    creation.config = view(cfg);
    creation.initial_persistent_memory = view(pm);
    expect(tessera_session_create_v1(&creation, &handle, &context));
  }
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  ~Session() {
    if (handle)
      tessera_session_close_v1(handle, &context);
  }
  tessera_session_info_v1 info() {
    auto v = init<tessera_session_info_v1>();
    expect(tessera_session_query_v1(handle, &v, &context));
    return v;
  }
  tessera_status step(unsigned mask = 0) {
    const auto i = info();
    auto a = inputs(mask);
    auto f = init<tessera_input_frame_v1>();
    f.epoch = i.frame_id.epoch;
    f.frame = i.frame_id.frame + 1;
    f.assignments = a.data();
    f.count = 8;
    return tessera_session_step_v1(handle, &f, &context);
  }
  Bytes state() {
    std::uint64_t n{};
    const auto s =
        tessera_state_export_canonical_v1(handle, {nullptr, 0}, &n, &context);
    if (s != TESSERA_BUFFER_TOO_SMALL)
      throw std::runtime_error("STATE_SIZE_QUERY_FAILED");
    Bytes b(static_cast<std::size_t>(n));
    expect(tessera_state_export_canonical_v1(handle, {b.data(), b.size()}, &n,
                                             &context));
    return b;
  }
  tessera_original_frame_v1 original() {
    auto v = init<tessera_original_frame_v1>();
    expect(tessera_original_frame_query_v1(handle, &v, &context));
    return v;
  }
  tessera_status submit(std::uint64_t idValue,
                        std::span<const tessera_control_item_v1> items) {
    auto i = info();
    auto t = init<tessera_control_transaction_v1>();
    t.epoch = i.frame_id.epoch;
    t.frame = i.frame_id.frame + 1;
    t.id = idValue;
    t.items = items.data();
    t.count = static_cast<std::uint32_t>(items.size());
    return tessera_controls_submit_v1(handle, &t, &context);
  }
  tessera_status observe(std::uint64_t idValue = 1) {
    const std::array<tessera_control_item_v1, 2> v{{{1, 1, 31}, {2, 1, 31}}};
    return submit(idValue, v);
  }
};
} // namespace fx01
