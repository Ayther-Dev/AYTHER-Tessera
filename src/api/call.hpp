#pragma once
#include "domain/result.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <type_traits>

namespace tessera::api {
constexpr std::uint64_t byte_limit = 64 * 1024 * 1024;
template <class T> T initialized() {
  T t{};
  t.header = {sizeof(T), 1, 0};
  return t;
}
inline tessera_status context_valid(const tessera_context_v1 *c) noexcept {
  if (!c || reinterpret_cast<std::uintptr_t>(c) % alignof(tessera_context_v1))
    return TESSERA_INVALID_ARGUMENT;
  if (c->header.size < sizeof(*c))
    return TESSERA_ABI_INCOMPATIBLE;
  if (c->header.major != 1 || c->header.minor != 0)
    return TESSERA_ABI_INCOMPATIBLE;
  if (c->header.size > std::numeric_limits<std::uintptr_t>::max() -
                           reinterpret_cast<std::uintptr_t>(c))
    return TESSERA_INVALID_ARGUMENT;
  return TESSERA_OK;
}
class Call {
  struct Range {
    std::uintptr_t begin, end;
  };
  std::array<Range, 20> ranges_{};
  std::size_t count_{};
  tessera_context_v1 *context_;
  tessera_header_v1 header_;
  std::uint32_t phase_;
  bool started_{};

public:
  bool can_write_context{true};
  Call(tessera_context_v1 *c, std::uint32_t p)
      : context_(c), header_(c->header), phase_(p) {
    range(c, c->header.size, alignof(tessera_context_v1));
  }
  void range(const void *p, std::uint64_t n, std::size_t alignment = 1) {
    require(n == 0 || p != nullptr);
    const auto address = reinterpret_cast<std::uintptr_t>(p);
    require(address % alignment == 0 &&
            n <= std::numeric_limits<std::uintptr_t>::max() - address);
    if (!n)
      return;
    require(count_ < ranges_.size());
    const Range r{address, address + static_cast<std::uintptr_t>(n)};
    for (std::size_t i = 0; i < count_; ++i)
      if (r.begin < ranges_[i].end && ranges_[i].begin < r.end) {
        can_write_context = false;
        throw Failure{TESSERA_INVALID_ARGUMENT};
      }
    ranges_[count_++] = r;
  }
  template <class T> void object(const T *p) {
    require(p != nullptr);
    range(p, sizeof(T), alignof(T));
    if constexpr (requires { p->header; }) {
      require(p->header.size >= sizeof(T) && p->header.major == 1 &&
                  p->header.minor == 0,
              TESSERA_ABI_INCOMPATIBLE);
      const auto begin = reinterpret_cast<std::uintptr_t>(p);
      require(p->header.size <=
              std::numeric_limits<std::uintptr_t>::max() - begin);
      const auto end = begin + p->header.size;
      for (std::size_t i = 0; i + 1 < count_; ++i)
        if (begin < ranges_[i].end && ranges_[i].begin < end) {
          can_write_context = false;
          throw Failure{TESSERA_INVALID_ARGUMENT};
        }
      ranges_[count_ - 1].end = end;
    }
  }
  void input(tessera_bytes_v1 b, std::uint64_t maximum = byte_limit) {
    require(b.bytes <= maximum);
    range(b.data, b.bytes);
  }
  void output(tessera_buffer_v1 b) {
    require(b.capacity <= byte_limit);
    range(b.data, b.capacity);
  }
  void start() {
    if (!started_) {
      *context_ = {};
      context_->header = header_;
      context_->phase = phase_;
      started_ = true;
    }
  }
  tessera_context_v1 &context() {
    start();
    return *context_;
  }
  tessera_status finish(tessera_status code, std::uint32_t phase = 0) {
    if (can_write_context) {
      start();
      context_->code = code;
      if (phase)
        context_->phase = phase;
    }
    return code;
  }
};
template <class F>
tessera_status invoke(tessera_context_v1 *context, std::uint32_t phase,
                      F &&f) noexcept {
  const auto status = context_valid(context);
  if (status != TESSERA_OK)
    return status;
  Call call(context, phase);
  try {
    return call.finish(f(call));
  } catch (const Failure &error) {
    return call.finish(error.code, error.phase);
  } catch (const std::bad_alloc &) {
    return call.finish(TESSERA_OUT_OF_MEMORY, phase);
  } catch (...) {
    return call.finish(TESSERA_BACKEND_FAILURE, phase);
  }
}
template <class T> void publish(T *out, T value) {
  value.header = out->header;
  *out = value;
}
inline tessera_status copy_bytes(std::span<const std::uint8_t> bytes,
                                 tessera_buffer_v1 out,
                                 std::uint64_t *required) {
  *required = bytes.size();
  if (out.capacity < bytes.size())
    return TESSERA_BUFFER_TOO_SMALL;
  if (!bytes.empty())
    std::memcpy(out.data, bytes.data(), bytes.size());
  return TESSERA_OK;
}
} // namespace tessera::api
