#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tessera {

using Bytes = std::vector<std::uint8_t>;
using InputState = std::array<std::int32_t, 8>;

struct RegionData {
  std::uint32_t kind{};
  std::uint32_t encoding{};
  std::uint64_t address{};
  std::uint32_t element_bytes{};
  std::uint32_t element_count{};
  std::uint32_t bank{};
  std::uint32_t flags{};
  Bytes data;
};

class IEmulatedSystem {
public:
  virtual ~IEmulatedSystem() = default;
  [[nodiscard]] virtual std::string_view backend_id() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t capabilities() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t frame() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t tick() const noexcept = 0;
  [[nodiscard]] virtual const InputState &latched_inputs() const noexcept = 0;
  [[nodiscard]] virtual const Bytes &pixels() const noexcept = 0;
  [[nodiscard]] virtual const Bytes &pcm() const noexcept = 0;
  [[nodiscard]] virtual bool original_available() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t system() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t frame_width() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t frame_height() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t frame_ticks() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t tick_hz() const noexcept = 0;
  virtual void prepare_observation(std::uint64_t) {}
  virtual void step(const InputState &inputs) = 0;
  [[nodiscard]] virtual Bytes machine_record() const = 0;
  virtual void restore_record(std::span<const std::uint8_t> record,
                              std::uint64_t frame,
                              const InputState &inputs) = 0;
  [[nodiscard]] virtual std::vector<RegionData>
  regions(std::uint64_t mask) const = 0;
};

} // namespace tessera
