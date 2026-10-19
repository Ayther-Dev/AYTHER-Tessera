#pragma once

#include "systems/emulated_system.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace tessera {

struct Content {
  std::uint32_t profile{};
  std::uint32_t initial_value{};
  std::uint32_t flags{};
};

[[nodiscard]] Content
parse_synthetic_content(std::span<const std::uint8_t> content);

class SyntheticBackend final : public IEmulatedSystem {
public:
  SyntheticBackend(std::string_view id, std::span<const std::uint8_t> content,
                   std::span<const std::uint8_t> initial_persistent_memory);

  [[nodiscard]] std::string_view backend_id() const noexcept override;
  [[nodiscard]] std::uint64_t capabilities() const noexcept override;
  [[nodiscard]] std::uint64_t frame() const noexcept override;
  [[nodiscard]] std::uint64_t tick() const noexcept override;
  [[nodiscard]] const InputState &latched_inputs() const noexcept override;
  [[nodiscard]] const Bytes &pixels() const noexcept override;
  [[nodiscard]] const Bytes &pcm() const noexcept override;
  [[nodiscard]] bool original_available() const noexcept override {
    return !pixels_.empty();
  }
  [[nodiscard]] std::uint32_t system() const noexcept override { return 0; }
  [[nodiscard]] std::uint32_t frame_width() const noexcept override { return 16; }
  [[nodiscard]] std::uint32_t frame_height() const noexcept override { return 16; }
  [[nodiscard]] std::uint64_t frame_ticks() const noexcept override { return 1000; }
  [[nodiscard]] std::uint64_t tick_hz() const noexcept override { return 60000; }
  void step(const InputState &inputs) override;
  [[nodiscard]] Bytes machine_record() const override;
  void restore_record(std::span<const std::uint8_t> record,
                      std::uint64_t frame, const InputState &inputs) override;
  [[nodiscard]] std::vector<RegionData>
  regions(std::uint64_t mask) const override;

private:
  std::uint32_t profile_{};
  std::uint32_t accumulator_{};
  std::array<std::uint8_t, 16> persistent_{};
  std::uint32_t audio_phase_{};
  std::uint64_t frame_{};
  InputState latched_{};
  Bytes pixels_;
  Bytes pcm_;
};

[[nodiscard]] std::unique_ptr<IEmulatedSystem>
make_synthetic_backend(std::string_view id,
                       std::span<const std::uint8_t> content,
                       std::span<const std::uint8_t> initial_persistent_memory);

} // namespace tessera
