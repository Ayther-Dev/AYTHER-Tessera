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
  void prepare_observation(std::uint64_t mask) override;
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
  Bytes temporal_indices_;
  Bytes temporal_coverage_;
  std::vector<std::array<std::uint16_t, 256>> temporal_palettes_;
  struct TemporalCell {
    std::array<std::uint8_t, 8 * 8 * 4> rgba{};
    std::uint64_t visible{};
    std::uint16_t tile_word{};
    std::uint8_t rows_seen{};
    bool observed{};
  };
  std::vector<TemporalCell> temporal_cells_;
  std::vector<VDP_ProvenancePixel> temporal_sources_;
  bool temporal_source_complete_{};
  std::array<bool, 1024> temporal_lines_{};
  bool temporal_requested_{};
  bool temporal_complete_{};
  Bytes pcm_;
  std::vector<cc_s16l> scratch_ = std::vector<cc_s16l>(32768);
  std::vector<std::int16_t> staging_;
  InputState input_{};
  std::uint32_t width_{}, height_{}, scanlines_{};
  std::uint64_t frame_{};

  static MegaDriveBackend &self(void *data) noexcept;
  void capture_provenance(std::uint32_t line,
                          const VDP_ProvenancePixel *sources,
                          std::uint32_t left, std::uint32_t right,
                          std::uint32_t width, std::uint32_t height);
  [[nodiscard]] std::array<std::uint8_t, 8 * 8 * 4>
  tile_rgba(std::uint16_t word) const;
  [[nodiscard]] RegionData temporal_region() const;
};

[[nodiscard]] std::unique_ptr<IEmulatedSystem>
make_megadrive_backend(std::string_view id, std::span<const std::uint8_t> content,
                       std::span<const std::uint8_t> initial_memory);

} // namespace tessera
