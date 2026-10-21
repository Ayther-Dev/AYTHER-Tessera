#pragma once

#include <nlohmann/json.hpp>
#include <picosha2.h>
#include <tessera/tessera.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tessera::cli {
using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;
using Json = nlohmann::json;
inline constexpr std::uint64_t file_limit = 64ULL * 1024 * 1024;

template <class T> T init() {
  T value{};
  value.header = {sizeof(T), 1, 0};
  return value;
}

struct ClientError final : std::exception {
  tessera_context_v1 context;
  std::string text;
  ClientError(tessera_context_v1 value, std::string message = {})
      : context(value), text(std::move(message)) {}
  const char *what() const noexcept override { return "TESSERA_CLI_FAILURE"; }
};

[[noreturn]] inline void fail(tessera_status code, std::uint32_t phase,
                              std::string text) {
  auto context = init<tessera_context_v1>();
  context.code = code;
  context.phase = phase;
  throw ClientError{context, std::move(text)};
}
inline void ensure(bool condition,
                   tessera_status code = TESSERA_INVALID_ARGUMENT,
                   std::string text = "Los datos no son válidos.") {
  if (!condition)
    fail(code, TESSERA_PHASE_VALIDATE, std::move(text));
}
inline void require_ok(tessera_status status,
                       const tessera_context_v1 &context) {
  if (status != TESSERA_OK)
    throw ClientError{context};
}
inline tessera_bytes_v1 view(View bytes) {
  return {bytes.data(), bytes.size()};
}
inline Bytes text_bytes(std::string_view text) {
  return {text.begin(), text.end()};
}
inline Bytes sha256(View bytes) {
  Bytes result(32);
  picosha2::hash256(bytes.begin(), bytes.end(), result.begin(), result.end());
  return result;
}
inline std::string hex_bytes(View bytes) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const auto byte : bytes) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}
inline std::string digest(View bytes) { return hex_bytes(sha256(bytes)); }
inline void put(Bytes &out, std::uint64_t value, unsigned count) {
  for (unsigned byte = 0; byte < count; ++byte)
    out.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
}
inline std::uint64_t read_le(View bytes, std::size_t offset, unsigned count) {
  ensure(count <= 8 && offset <= bytes.size() && count <= bytes.size() - offset,
         TESSERA_INVALID_TRACE, "La traza está truncada.");
  std::uint64_t value = 0;
  for (unsigned byte = 0; byte < count; ++byte)
    value |= std::uint64_t{bytes[offset + byte]} << (byte * 8);
  return value;
}
inline void field(Bytes &out, std::uint32_t tag, std::uint32_t encoding,
                  View payload) {
  put(out, tag, 4);
  put(out, encoding, 4);
  put(out, payload.size(), 8);
  out.insert(out.end(), payload.begin(), payload.end());
}
inline void scalar(Bytes &out, std::uint32_t tag, std::uint32_t encoding,
                   std::uint64_t value) {
  Bytes payload;
  put(payload, value, encoding == 1 || encoding == 2 ? 4U : 8U);
  field(out, tag, encoding, payload);
}
inline Bytes array(const std::vector<Bytes> &entries) {
  Bytes out;
  put(out, entries.size(), 8);
  for (const auto &entry : entries) {
    put(out, entry.size(), 8);
    out.insert(out.end(), entry.begin(), entry.end());
  }
  return out;
}
template <std::size_t N> std::string fixed_string(const char (&value)[N]) {
  const auto end = std::find(value, value + N, '\0');
  ensure(end != value + N, TESSERA_BACKEND_FAILURE,
         "El backend devolvió una identidad inválida.");
  return {value, end};
}
inline std::vector<tessera_backend_info_v1> backends() {
  auto context = init<tessera_context_v1>();
  std::uint32_t required = 0;
  const auto first =
      tessera_backends_enumerate_v1(nullptr, 0, &required, &context);
  if (first != TESSERA_BUFFER_TOO_SMALL && first != TESSERA_OK)
    throw ClientError{context};
  ensure(required <= 1024, TESSERA_BACKEND_FAILURE);
  std::vector<tessera_backend_info_v1> result(required);
  for (auto &item : result)
    item = init<tessera_backend_info_v1>();
  require_ok(tessera_backends_enumerate_v1(result.data(), required, &required,
                                           &context),
             context);
  return result;
}
inline tessera_backend_info_v1 find_backend(std::string_view id) {
  for (const auto &backend : backends())
    if (fixed_string(backend.id) == id)
      return backend;
  fail(TESSERA_IMPLEMENTATION_UNAVAILABLE, TESSERA_PHASE_CREATE,
       "El backend solicitado no está disponible.");
}
inline Bytes demo_content(std::string_view id) {
  ensure(id == "synthetic.minimal" || id == "synthetic.complete");
  auto result = text_bytes("TSSYN001");
  for (const auto value : {id == "synthetic.minimal" ? 1U : 2U, 7U, 0U, 0U})
    put(result, value, 4);
  return result;
}
inline Bytes demo_memory() {
  Bytes result;
  for (unsigned index = 0; index < 16; ++index)
    result.push_back(static_cast<std::uint8_t>(3 + 17 * index));
  return result;
}
inline Bytes demo_config(const tessera_backend_info_v1 &backend, View memory) {
  Bytes result;
  scalar(result, 1, 1, 0);
  field(result, 2, 6, text_bytes(fixed_string(backend.id)));
  field(result, 3, 5, backend.build_id);
  scalar(result, 4, 1, 0);
  scalar(result, 5, 1, 48000);
  std::vector<Bytes> controls;
  for (unsigned index = 0; index < 8; ++index) {
    Bytes item;
    scalar(item, 1, 1, 0);
    scalar(item, 2, 1, index);
    scalar(item, 3, 1, 1);
    scalar(item, 4, 2, 0);
    scalar(item, 5, 2, 1);
    controls.push_back(std::move(item));
  }
  field(result, 6, 8, array(controls));
  field(result, 7, 5, sha256(memory));
  return result;
}
inline std::array<tessera_input_assignment_v1, 8>
digital_inputs(std::uint32_t mask = 0) {
  std::array<tessera_input_assignment_v1, 8> result{};
  for (std::uint16_t index = 0; index < 8; ++index)
    result[index] = {0, index, static_cast<std::int32_t>((mask >> index) & 1U)};
  return result;
}

class PublicSession {
public:
  PublicSession(std::string_view id, View content, View config, View memory) {
    auto creation = init<tessera_session_create_info_v1>();
    const auto id_bytes = text_bytes(id);
    creation.backend_id = view(id_bytes);
    creation.system_override =
        id == "clownmdemu" ? TESSERA_SYSTEM_MEGA_DRIVE : TESSERA_SYSTEM_NONE;
    creation.content = view(content);
    creation.config = view(config);
    creation.initial_persistent_memory = view(memory);
    require_ok(tessera_session_create_v1(&creation, &handle_, &context_),
               context_);
  }
  PublicSession(const PublicSession &) = delete;
  PublicSession &operator=(const PublicSession &) = delete;
  ~PublicSession() {
    if (handle_ != 0)
      (void)tessera_session_close_v1(handle_, &context_);
  }
  [[nodiscard]] tessera_session handle() const noexcept { return handle_; }
  [[nodiscard]] const tessera_context_v1 &context() const noexcept {
    return context_;
  }
  tessera_session_info_v1 info() {
    auto result = init<tessera_session_info_v1>();
    require_ok(tessera_session_query_v1(handle_, &result, &context_), context_);
    return result;
  }
  void pause() {
    require_ok(tessera_session_pause_v1(handle_, &context_), context_);
  }
  void reset() {
    require_ok(tessera_session_reset_v1(handle_, &context_), context_);
  }
  void step(const std::array<tessera_input_assignment_v1, 8> &inputs) {
    const auto current = info();
    auto frame = init<tessera_input_frame_v1>();
    frame.epoch = current.frame_id.epoch;
    frame.frame = current.frame_id.frame + 1;
    frame.assignments = inputs.data();
    frame.count = static_cast<std::uint32_t>(inputs.size());
    require_ok(tessera_session_step_v1(handle_, &frame, &context_), context_);
  }
  void bind(View trace) {
    require_ok(tessera_session_bind_trace_v1(handle_, view(trace), &context_),
               context_);
  }
  tessera_status replay_next() {
    return tessera_session_replay_next_v1(handle_, &context_);
  }
  void submit(std::uint64_t id,
              std::span<const tessera_control_item_v1> items) {
    const auto current = info();
    auto transaction = init<tessera_control_transaction_v1>();
    transaction.epoch = current.frame_id.epoch;
    transaction.frame = current.frame_id.frame + 1;
    transaction.id = id;
    transaction.items = items.data();
    transaction.count = static_cast<std::uint32_t>(items.size());
    require_ok(tessera_controls_submit_v1(handle_, &transaction, &context_),
               context_);
  }
  Bytes state() {
    std::uint64_t required = 0;
    const auto first = tessera_state_export_canonical_v1(handle_, {nullptr, 0},
                                                         &required, &context_);
    if (first != TESSERA_BUFFER_TOO_SMALL)
      require_ok(first, context_);
    ensure(required <= file_limit, TESSERA_BUDGET_EXCEEDED);
    Bytes result(static_cast<std::size_t>(required));
    require_ok(tessera_state_export_canonical_v1(handle_,
                                                 {result.data(), result.size()},
                                                 &required, &context_),
               context_);
    return result;
  }
  tessera_original_frame_v1 original() {
    auto result = init<tessera_original_frame_v1>();
    require_ok(tessera_original_frame_query_v1(handle_, &result, &context_),
               context_);
    return result;
  }
  void close() {
    require_ok(tessera_session_close_v1(handle_, &context_), context_);
    handle_ = 0;
  }

private:
  tessera_session handle_{};
  tessera_context_v1 context_ = init<tessera_context_v1>();
};

inline Bytes canonical_video(const tessera_original_frame_v1 &frame) {
  ensure(frame.width > 0 && frame.height > 0 && frame.width <= 1024 &&
             frame.height <= 1024 && frame.video_format == TESSERA_VIDEO_RGBA8,
         TESSERA_BACKEND_FAILURE);
  const auto row = std::uint64_t{frame.width} * 4;
  ensure(frame.row_bytes >= row && frame.pixels.data != nullptr &&
             frame.pixels.bytes >=
                 std::uint64_t{frame.row_bytes} * frame.height,
         TESSERA_BACKEND_FAILURE);
  auto result = text_bytes("TSVIDEO1");
  put(result, frame.width, 4);
  put(result, frame.height, 4);
  put(result, 1, 4);
  for (std::uint32_t y = 0; y < frame.height; ++y) {
    const auto offset =
        static_cast<std::size_t>(std::uint64_t{frame.row_bytes} * y);
    result.insert(result.end(), frame.pixels.data + offset,
                  frame.pixels.data + offset + row);
  }
  return result;
}
inline Bytes canonical_audio(const tessera_original_frame_v1 &frame) {
  ensure(frame.sample_rate > 0 && frame.channels == 2 &&
             frame.pcm_frames <= 16384 &&
             frame.pcm.bytes == frame.pcm_frames * 4 &&
             (frame.pcm.bytes == 0 || frame.pcm.data != nullptr),
         TESSERA_BACKEND_FAILURE);
  auto result = text_bytes("TSAUDIO1");
  put(result, frame.sample_rate, 4);
  put(result, 2, 4);
  put(result, frame.pcm_frames, 8);
  if (frame.pcm.bytes != 0)
    result.insert(result.end(), frame.pcm.data,
                  frame.pcm.data + frame.pcm.bytes);
  return result;
}
struct CanonicalDigests {
  std::string state, video, audio;
};
inline CanonicalDigests canonical_digests(PublicSession &session) {
  const auto state = session.state();
  const auto original = session.original();
  return {digest(state), digest(canonical_video(original)),
          digest(canonical_audio(original))};
}
inline std::string status_text(const tessera_context_v1 &context) {
  std::uint64_t required = 0;
  const auto first =
      tessera_status_format_v1(&context, {nullptr, 0}, &required);
  if (first != TESSERA_BUFFER_TOO_SMALL || required > 65536)
    return "La operación no pudo completarse.";
  Bytes text(static_cast<std::size_t>(required));
  if (tessera_status_format_v1(&context, {text.data(), text.size()},
                               &required) != TESSERA_OK)
    return "La operación no pudo completarse.";
  return {text.begin(), text.end()};
}
} // namespace tessera::cli
