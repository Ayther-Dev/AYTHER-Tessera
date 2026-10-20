#include "backends/megadrive/megadrive_backend.hpp"

#include "backends/megadrive/clown_state_codec.hpp"
#include "domain/result.hpp"
#include "domain/wire.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace tessera {
namespace {
constexpr std::uint64_t megadrive_capabilities = 0x1FFF;
constexpr std::uint64_t supported_regions = 31;
constexpr std::array<ClownMDEmu_Button, 8> buttons{
    CLOWNMDEMU_BUTTON_UP, CLOWNMDEMU_BUTTON_DOWN, CLOWNMDEMU_BUTTON_LEFT,
    CLOWNMDEMU_BUTTON_RIGHT, CLOWNMDEMU_BUTTON_A, CLOWNMDEMU_BUTTON_B,
    CLOWNMDEMU_BUTTON_C, CLOWNMDEMU_BUTTON_START};

void append_u16(Bytes &output, std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value));
  output.push_back(static_cast<std::uint8_t>(value >> 8));
}

RegionData region(std::uint32_t kind, std::uint32_t element_bytes,
                  std::uint32_t element_count, Bytes data) {
  return {kind, TESSERA_ENCODING_BYTES, 0, element_bytes,
          element_count, 0, 0, std::move(data)};
}
} // namespace

MegaDriveBackend &MegaDriveBackend::self(void *data) noexcept {
  return *static_cast<MegaDriveBackend *>(data);
}

MegaDriveBackend::MegaDriveBackend(std::span<const std::uint8_t> content,
                                   std::span<const std::uint8_t> initial_memory) {
  require(content.size() >= 512 && content.size() % 2 == 0 &&
              content.size() <= std::numeric_limits<cc_u32f>::max() * 2ULL,
          TESSERA_INVALID_CONTENT, TESSERA_PHASE_CREATE);
  require(initial_memory.size() == 65536 &&
              std::all_of(initial_memory.begin(), initial_memory.end(),
                          [](std::uint8_t byte) { return byte == 0; }),
          TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_CREATE);
  rom_.reserve(content.size() / 2);
  for (std::size_t index = 0; index < content.size(); index += 2)
    rom_.push_back(static_cast<cc_u16l>((content[index] << 8) |
                                         content[index + 1]));

  callbacks_.user_data = this;
  callbacks_.colour_updated = [](void *data, cc_u16f index, cc_u16f colour) {
    auto &backend = self(data);
    require(index < backend.palette_.size(), TESSERA_BACKEND_FAILURE,
            TESSERA_PHASE_STEP);
    backend.palette_[index] = static_cast<std::uint16_t>(colour);
  };
  callbacks_.scanline_rendered = [](void *data, cc_u16f line,
                                    const cc_u8l *row, cc_u16f left,
                                    cc_u16f right,
                                    cc_u16f width, cc_u16f height) {
    auto &backend = self(data);
    require(width <= 1024 && height <= 1024 && line < height &&
                left <= right && right <= width,
            TESSERA_BACKEND_FAILURE, TESSERA_PHASE_STEP);
    if (backend.scanlines_ != 0 &&
        (backend.width_ != width || backend.height_ != height))
      backend.temporal_complete_ = false;
    backend.width_ = width;
    backend.height_ = height;
    ++backend.scanlines_;
    if (backend.temporal_requested_) {
      backend.temporal_lines_[line] = true;
      std::copy(row + left, row + right,
                backend.temporal_indices_.data() +
                    std::size_t(line) * 1024 + left);
      std::fill(backend.temporal_coverage_.begin() +
                    std::size_t(line) * 1024 + left,
                backend.temporal_coverage_.begin() +
                    std::size_t(line) * 1024 + right, 1);
      backend.temporal_palettes_[line] = backend.palette_;
    }
    for (std::size_t x = left; x < right; ++x) {
      const auto colour = backend.palette_[row[x]];
      const auto offset = (std::size_t(line) * width + x) * 4;
      backend.pixels_buffer_[offset] = static_cast<std::uint8_t>((colour & 15) * 17);
      backend.pixels_buffer_[offset + 1] =
          static_cast<std::uint8_t>(((colour >> 4) & 15) * 17);
      backend.pixels_buffer_[offset + 2] =
          static_cast<std::uint8_t>(((colour >> 8) & 15) * 17);
      backend.pixels_buffer_[offset + 3] = 255;
    }
  };
  machine_->vdp.provenance_callback = nullptr;
  callbacks_.input_requested = [](void *data, cc_u8f player,
                                  ClownMDEmu_Button button) -> cc_bool {
    const auto &backend = self(data);
    if (player != 0)
      return cc_false;
    for (std::size_t index = 0; index < buttons.size(); ++index)
      if (button == buttons[index])
        return backend.input_[index] != 0 ? cc_true : cc_false;
    return cc_false;
  };
  callbacks_.fm_audio_to_be_generated =
      [](void *data, ClownMDEmu *machine, std::size_t count,
         void (*generate)(ClownMDEmu *, cc_s16l *, std::size_t)) {
        auto &backend = self(data);
        require(count * 2 <= backend.scratch_.size(), TESSERA_BACKEND_FAILURE,
                TESSERA_PHASE_STEP);
        std::fill_n(backend.scratch_.data(), count * 2, 0);
        generate(machine, backend.scratch_.data(), count);
        backend.staging_.assign(backend.scratch_.begin(),
                                backend.scratch_.begin() + count * 2);
        backend.audio_.push_fm(backend.staging_);
      };
  callbacks_.psg_audio_to_be_generated =
      [](void *data, ClownMDEmu *machine, std::size_t count,
         void (*generate)(ClownMDEmu *, cc_s16l *, std::size_t)) {
        auto &backend = self(data);
        require(count <= backend.scratch_.size(), TESSERA_BACKEND_FAILURE,
                TESSERA_PHASE_STEP);
        std::fill_n(backend.scratch_.data(), count, 0);
        generate(machine, backend.scratch_.data(), count);
        backend.staging_.assign(backend.scratch_.begin(),
                                backend.scratch_.begin() + count);
        backend.audio_.push_psg(backend.staging_);
      };
  const auto silent_cd =
      [](void *data, ClownMDEmu *machine, std::size_t count,
         void (*generate)(ClownMDEmu *, cc_s16l *, std::size_t)) {
        auto &backend = self(data);
        require(count * 2 <= backend.scratch_.size(), TESSERA_BACKEND_FAILURE,
                TESSERA_PHASE_STEP);
        std::fill_n(backend.scratch_.data(), count * 2, 0);
        generate(machine, backend.scratch_.data(), count);
        require(std::all_of(backend.scratch_.begin(),
                            backend.scratch_.begin() + count * 2,
                            [](auto value) { return value == 0; }),
                TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_STEP);
      };
  callbacks_.pcm_audio_to_be_generated = silent_cd;
  callbacks_.cdda_audio_to_be_generated = silent_cd;
  callbacks_.cd_audio_read = [](void *, cc_s16l *samples,
                                std::size_t count) -> std::size_t {
    std::fill_n(samples, count * 2, 0);
    return count;
  };

  ClownMDEmu_InitialConfiguration config{};
  config.general.region = CLOWNMDEMU_REGION_OVERSEAS;
  config.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
  config.general.cd_add_on_enabled = cc_false;
  config.controller_manager.protocol = CONTROLLER_MANAGER_PROTOCOL_STANDARD;
  config.fm.ladder_effect_disabled = cc_true;
  ClownMDEmu_Initialise(machine_.get(), &config, &callbacks_);
  ClownMDEmu_SetCartridge(machine_.get(), rom_.data(),
                          static_cast<cc_u32f>(rom_.size()));
  ClownMDEmu_HardReset(machine_.get(), cc_true, cc_false);
}

std::string_view MegaDriveBackend::backend_id() const noexcept {
  return "clownmdemu";
}
std::uint64_t MegaDriveBackend::capabilities() const noexcept {
  return megadrive_capabilities;
}
std::uint64_t MegaDriveBackend::frame() const noexcept { return frame_; }
std::uint64_t MegaDriveBackend::tick() const noexcept {
  return frame_ * frame_ticks();
}
const InputState &MegaDriveBackend::latched_inputs() const noexcept {
  return input_;
}
const Bytes &MegaDriveBackend::pixels() const noexcept { return pixels_buffer_; }
const Bytes &MegaDriveBackend::pcm() const noexcept { return pcm_; }

void MegaDriveBackend::prepare_observation(std::uint64_t mask) {
  temporal_requested_ = (mask & TESSERA_REGION_TEMPORAL) != 0;
  machine_->vdp.provenance_callback = temporal_requested_
      ? +[](void *data, cc_u16f line, const VDP_ProvenancePixel *sources,
            cc_u16f left, cc_u16f right, cc_u16f width, cc_u16f height) {
          self(data).capture_provenance(line, sources, left, right, width,
                                        height);
        }
      : nullptr;
  machine_->vdp.provenance_user_data = temporal_requested_ ? this : nullptr;
  if (temporal_requested_) {
    temporal_indices_.resize(1024 * 1024);
    temporal_coverage_.resize(1024 * 1024);
    temporal_palettes_.resize(1024);
  }
}

void MegaDriveBackend::step(const InputState &inputs) {
  require(frame_ < megadrive::CanonicalAudio::max_frames,
          TESSERA_BUDGET_EXCEEDED, TESSERA_PHASE_STEP);
  require(!(inputs[0] && inputs[1]) && !(inputs[2] && inputs[3]),
          TESSERA_INPUT_CONTRADICTORY, TESSERA_PHASE_STEP);
  input_ = inputs;
  scanlines_ = 0;
  temporal_lines_.fill(false);
  temporal_complete_ = temporal_requested_;
  temporal_source_complete_ = temporal_requested_;
  temporal_cells_.clear();
  temporal_sources_.clear();
  if (temporal_requested_)
    std::fill(temporal_coverage_.begin(), temporal_coverage_.end(), 0);
  pixels_buffer_.resize(1024 * 1024 * 4);
  ClownMDEmu_Iterate(machine_.get());
  auto next_pcm = audio_.finish_frame();
  require(scanlines_ > 0 && width_ > 0 && height_ > 0 &&
              next_pcm.size() <= 16384 * 4,
          TESSERA_BACKEND_FAILURE, TESSERA_PHASE_STEP);
  pixels_buffer_.resize(std::size_t(width_) * height_ * 4);
  if (temporal_complete_)
    for (std::size_t line = 0; line < height_; ++line)
      if (!temporal_lines_[line] ||
          !std::all_of(temporal_coverage_.begin() + line * 1024,
                       temporal_coverage_.begin() + line * 1024 + width_,
                       [](std::uint8_t covered) { return covered != 0; })) {
        temporal_complete_ = false;
        break;
      }
  if (temporal_source_complete_ &&
      temporal_sources_.size() != std::size_t(width_) * height_)
    temporal_source_complete_ = false;
  if (temporal_source_complete_)
    for (const auto &cell : temporal_cells_)
      if (cell.observed && cell.rows_seen != 0xFF) {
        temporal_source_complete_ = false;
        break;
      }
  pcm_.swap(next_pcm);
  ++frame_;
}

Bytes MegaDriveBackend::machine_record() const {
  Bytes record;
  const auto machine = clown_state_codec::encode_machine(*machine_);
  const auto audio = audio_.encode_state();
  wire::field(record, 1, TESSERA_ENCODING_BYTES, machine);
  wire::field(record, 2, TESSERA_ENCODING_BYTES, audio);
  return record;
}

void MegaDriveBackend::restore_record(
    std::span<const std::uint8_t> encoded, std::uint64_t frame,
    const InputState &inputs) {
  require(frame <= megadrive::CanonicalAudio::max_frames &&
              !(inputs[0] && inputs[1]) && !(inputs[2] && inputs[3]),
          TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
  const auto fields = wire::record(encoded, TESSERA_CHECKPOINT_INVALID,
                                   TESSERA_PHASE_RESTORE);
  require(fields.size() == 2, TESSERA_CHECKPOINT_INVALID,
          TESSERA_PHASE_RESTORE);
  const auto machine_bytes =
      wire::get(fields, 0, 1, TESSERA_ENCODING_BYTES,
                std::numeric_limits<std::size_t>::max(),
                TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
  const auto audio_bytes = wire::get(
      fields, 1, 2, TESSERA_ENCODING_BYTES,
      std::numeric_limits<std::size_t>::max(), TESSERA_CHECKPOINT_INVALID,
      TESSERA_PHASE_RESTORE);
  try {
    clown_state_codec::decode_machine(*machine_, machine_bytes);
    audio_.decode_state(audio_bytes);
  } catch (const std::exception &) {
    throw Failure{TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE};
  }
  require(audio_.frames_completed() == frame &&
              !machine_->state.mega_cd.cd_inserted &&
              !machine_->mega_cd.pcm.state.sounding &&
              !machine_->mega_cd.cdda.state.playing,
          TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
  input_ = inputs;
  frame_ = frame;
  for (std::size_t index = 0; index < 64; ++index) {
    const auto colour = machine_->vdp.state.cram[index];
    palette_[index] = static_cast<std::uint16_t>(
        colour | ((colour & 0x888) >> 3));
    palette_[64 + index] = static_cast<std::uint16_t>(colour >> 1);
    palette_[128 + index] = static_cast<std::uint16_t>(0x888 + (colour >> 1));
  }
  width_ = height_ = scanlines_ = 0;
  temporal_complete_ = false;
  temporal_source_complete_ = false;
  temporal_lines_.fill(false);
  pcm_.clear();
}

std::vector<RegionData> MegaDriveBackend::regions(std::uint64_t mask) const {
  require((mask & ~supported_regions) == 0, TESSERA_UNSUPPORTED_CAPABILITY,
          TESSERA_PHASE_READ);
  if (!mask)
    return {};
  require(frame_ > 0, TESSERA_WRONG_STATE, TESSERA_PHASE_READ);
  std::vector<RegionData> result;
  if (mask & TESSERA_REGION_MEMORY) {
    Bytes bytes;
    bytes.reserve(131072);
    for (auto word : machine_->state.m68k.ram)
      append_u16(bytes, word);
    bytes.insert(bytes.end(), std::begin(machine_->vdp.state.vram),
                 std::end(machine_->vdp.state.vram));
    result.push_back(region(1, 1, static_cast<std::uint32_t>(bytes.size()),
                            std::move(bytes)));
  }
  if (mask & TESSERA_REGION_PALETTE) {
    Bytes bytes;
    for (auto word : machine_->vdp.state.cram)
      append_u16(bytes, word);
    result.push_back(region(2, 2, static_cast<std::uint32_t>(bytes.size() / 2),
                            std::move(bytes)));
  }
  if (mask & TESSERA_REGION_SPRITES) {
    Bytes bytes(std::begin(machine_->vdp.state.vram),
                std::end(machine_->vdp.state.vram));
    result.push_back(region(3, 1, static_cast<std::uint32_t>(bytes.size()),
                            std::move(bytes)));
  }
  if (mask & TESSERA_REGION_GEOMETRY) {
    Bytes bytes;
    wire::scalar(bytes, 1, TESSERA_ENCODING_U32, width_);
    wire::scalar(bytes, 2, TESSERA_ENCODING_U32, height_);
    result.push_back(region(4, 1, static_cast<std::uint32_t>(bytes.size()),
                            std::move(bytes)));
  }
  if (mask & TESSERA_REGION_TEMPORAL) {
    result.push_back(temporal_region());
  }
  return result;
}

std::unique_ptr<IEmulatedSystem>
make_megadrive_backend(std::string_view id,
                       std::span<const std::uint8_t> content,
                       std::span<const std::uint8_t> initial_memory) {
  require(id == "clownmdemu", TESSERA_IMPLEMENTATION_UNAVAILABLE,
          TESSERA_PHASE_CREATE);
  return std::make_unique<MegaDriveBackend>(content, initial_memory);
}

} // namespace tessera
