#pragma once

#include "backends/megadrive/canonical_audio.hpp"
#include "systems/emulated_system.hpp"

#include <clownmdemu.h>
#include <array>
#include <memory>
#include <span>

namespace tessera {

class MegaDriveBackend final : public IEmulatedSystem {
public:
  MegaDriveBackend(std::span<const std::uint8_t> content,
                   std::span<const std::uint8_t> initial_memory);

  [[nodiscard]] std::string_view backend_id() const noexcept override;
  [[nodiscard]] std::uint64_t capabilities() const noexcept override;
  [[nodiscard]] std::uint64_t frame() const noexcept override;
  [[nodiscard]] std::uint64_t tick() const noexcept override;
  [[nodiscard]] const InputState &latched_inputs() const noexcept override;
  [[nodiscard]] const Bytes &pixels() const noexcept override;
  [[nodiscard]] const Bytes &pcm() const noexcept override;
  [[nodiscard]] bool original_available() const noexcept override {
    return width_ != 0 && height_ != 0 && !pcm_.empty();
  }
  [[nodiscard]] std::uint32_t system() const noexcept override { return 1; }
  [[nodiscard]] std::uint32_t frame_width() const noexcept override { return width_; }
  [[nodiscard]] std::uint32_t frame_height() const noexcept override { return height_; }
  [[nodiscard]] std::uint64_t frame_ticks() const noexcept override {
    return megadrive::CanonicalAudio::master_ticks_per_frame;
  }
  [[nodiscard]] std::uint64_t tick_hz() const noexcept override {
    return megadrive::CanonicalAudio::master_clock_hz;
  }
  void step(const InputState &inputs) override;
  [[nodiscard]] Bytes machine_record() const override;
  void restore_record(std::span<const std::uint8_t> record,
                      std::uint64_t frame, const InputState &inputs) override;
  [[nodiscard]] std::vector<RegionData> regions(std::uint64_t mask) const override;

private:
  std::unique_ptr<ClownMDEmu> machine_ = std::make_unique<ClownMDEmu>();
  ClownMDEmu_Callbacks callbacks_{};
  std::vector<cc_u16l> rom_;
  megadrive::CanonicalAudio audio_{{48000, 32768, 4096}};
  std::array<std::uint16_t, 256> palette_{};
  Bytes pixels_buffer_ = Bytes(1024 * 1024 * 4);
  Bytes pcm_;
  std::vector<cc_s16l> scratch_ = std::vector<cc_s16l>(32768);
  std::vector<std::int16_t> staging_;
  InputState input_{};
  std::uint32_t width_{}, height_{}, scanlines_{};
  std::uint64_t frame_{};

  static MegaDriveBackend &self(void *data) noexcept;
};

[[nodiscard]] std::unique_ptr<IEmulatedSystem>
make_megadrive_backend(std::string_view id, std::span<const std::uint8_t> content,
                       std::span<const std::uint8_t> initial_memory);

} // namespace tessera
