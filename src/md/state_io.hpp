// Byte stream of the native Mega Drive savestate (spec 008, TP-110.1,
// docs/savestate-v1.md): fixed field order, little endian, no structure copies.
// The reader never throws: a short or inconsistent stream clears `ok()` and the
// reads that follow return zero, so a loader checks once at the end.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

class StateWriter {
public:
  explicit StateWriter(std::vector<std::uint8_t> &out) : out_(out) {}

  void u8(std::uint8_t v) { out_.push_back(v); }
  void boolean(bool v) { u8(v ? 1 : 0); }
  void u16(std::uint16_t v) { le(v, 2); }
  void u32(std::uint32_t v) { le(v, 4); }
  void u64(std::uint64_t v) { le(v, 8); }
  void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
  void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
  void bytes(std::span<const std::uint8_t> data) {
    out_.insert(out_.end(), data.begin(), data.end());
  }
  template <std::size_t N> void array(const std::array<std::uint8_t, N> &a) {
    bytes(a);
  }
  template <std::size_t N> void array(const std::array<std::uint16_t, N> &a) {
    for (const auto v : a)
      u16(v);
  }
  template <std::size_t N> void array(const std::array<std::uint64_t, N> &a) {
    for (const auto v : a)
      u64(v);
  }
  /// A length-prefixed block.
  void block(std::span<const std::uint8_t> data) {
    u32(static_cast<std::uint32_t>(data.size()));
    bytes(data);
  }

private:
  void le(std::uint64_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  std::vector<std::uint8_t> &out_;
};

class StateReader {
public:
  explicit StateReader(std::span<const std::uint8_t> in) : in_(in) {}

  bool ok() const noexcept { return ok_; }
  /// All input consumed and no read failed.
  bool finished() const noexcept { return ok_ && at_ == in_.size(); }
  std::size_t remaining() const noexcept { return in_.size() - at_; }
  void fail() noexcept { ok_ = false; }

  std::uint8_t u8() { return static_cast<std::uint8_t>(le(1)); }
  bool boolean() {
    const auto v = u8();
    if (v > 1)
      ok_ = false;
    return v == 1;
  }
  std::uint16_t u16() { return static_cast<std::uint16_t>(le(2)); }
  std::uint32_t u32() { return static_cast<std::uint32_t>(le(4)); }
  std::uint64_t u64() { return le(8); }
  std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
  std::int64_t i64() { return static_cast<std::int64_t>(u64()); }
  void bytes(std::span<std::uint8_t> out) {
    if (!take(out.size())) {
      std::fill(out.begin(), out.end(), std::uint8_t{0});
      return;
    }
    std::copy_n(in_.begin() + static_cast<std::ptrdiff_t>(at_), out.size(),
                out.begin());
    at_ += out.size();
  }
  template <std::size_t N> void array(std::array<std::uint8_t, N> &a) {
    bytes(a);
  }
  template <std::size_t N> void array(std::array<std::uint16_t, N> &a) {
    for (auto &v : a)
      v = u16();
  }
  template <std::size_t N> void array(std::array<std::uint64_t, N> &a) {
    for (auto &v : a)
      v = u64();
  }
  /// A length-prefixed block of at most `limit` bytes (empty on failure).
  std::span<const std::uint8_t> block(std::size_t limit) {
    const auto size = u32();
    if (!ok_ || size > limit || !take(size)) {
      ok_ = false;
      return {};
    }
    const auto view = in_.subspan(at_, size);
    at_ += size;
    return view;
  }

private:
  bool take(std::size_t n) noexcept {
    if (!ok_ || n > in_.size() - at_) {
      ok_ = false;
      return false;
    }
    return true;
  }
  std::uint64_t le(unsigned n) {
    if (!take(n))
      return 0;
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
      v |= std::uint64_t{in_[at_ + i]} << (8 * i);
    at_ += n;
    return v;
  }
  std::span<const std::uint8_t> in_;
  std::size_t at_ = 0;
  bool ok_ = true;
};

} // namespace tessera::md
