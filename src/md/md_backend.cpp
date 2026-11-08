#include "md/md_backend.hpp"

#include "domain/result.hpp"
#include "domain/wire.hpp"
#include "md/cart.hpp"
#include "md/input.hpp"
#include "md/region.hpp"
#include "md/regions.hpp"
#include "md/system.hpp"
#include "observation/region_table.hpp"
#include "observation/subscriptions.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace tessera::md {
namespace {
/// FRAME | INPUT | VIDEO | PCM | CHECKPOINT. The observation regions arrive
/// with their own tasks (TP-2xx) and are not claimed until then.
constexpr std::uint64_t kCapabilities = 0x20F;
/// A session this long would overflow the tick counters long before; the
/// bound keeps `tick()` exact in 64 bits.
constexpr std::uint64_t kMaxFrames = 1ULL << 40;

/// The ROM image whose header holds the country field: the content itself
/// when it is plain, the unwrapped one for a copier header or .smd.
std::vector<std::uint8_t> plain_header_image(wire::View content) {
  try {
    return load_cart(content).rom;
  } catch (const Failure &) {
    return {}; // the System refuses it; the region falls back to Americas
  }
}

class NativeBackend final : public IEmulatedSystem, public IObservable {
public:
  NativeBackend(wire::View content, wire::View memory, RegionChoice choice) {
    require(content.size() >= 512 && content.size() % 2 == 0 &&
                content.size() <= 8U * 1024U * 1024U,
            TESSERA_INVALID_CONTENT, TESSERA_PHASE_CREATE);
    require(memory.size() == 65536 &&
                std::all_of(memory.begin(), memory.end(),
                            [](std::uint8_t byte) { return byte == 0; }),
            TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_CREATE);
    std::vector<std::uint8_t> rom(content.begin(), content.end());
    // TP-108.4: the header chooses the console unless the caller fixed it.
    auto config = region_config(choice, plain_header_image(content));
    config.pads = {PadType::six_button, PadType::six_button};
    standard_ = config.standard;
    system_ = std::make_unique<System>(std::move(rom), config);
    system_->reset();
    refresh_frame_geometry();
  }

  [[nodiscard]] std::string_view backend_id() const noexcept override {
    return "megadrive";
  }
  [[nodiscard]] std::uint64_t capabilities() const noexcept override {
    return kCapabilities;
  }
  [[nodiscard]] std::uint64_t frame() const noexcept override { return frame_; }
  [[nodiscard]] std::uint64_t tick() const noexcept override {
    return frame_ * frame_ticks();
  }
  [[nodiscard]] const InputState &latched_inputs() const noexcept override {
    return input_;
  }
  [[nodiscard]] InputLayout input_layout() const noexcept override {
    return kPadsInputLayout;
  }
  [[nodiscard]] const Bytes &pixels() const noexcept override {
    return pixels_;
  }
  [[nodiscard]] const Bytes &pcm() const noexcept override { return pcm_; }
  [[nodiscard]] bool original_available() const noexcept override {
    return frame_ != 0 && !pixels_.empty();
  }
  [[nodiscard]] std::uint32_t system() const noexcept override { return 1; }
  [[nodiscard]] std::uint32_t frame_width() const noexcept override {
    return width_;
  }
  [[nodiscard]] std::uint32_t frame_height() const noexcept override {
    return height_;
  }
  [[nodiscard]] std::uint64_t frame_ticks() const noexcept override {
    return cycles_per_frame(standard_);
  }
  [[nodiscard]] std::uint64_t tick_hz() const noexcept override {
    return master_hz(standard_);
  }

  void step(const InputState &inputs) override {
    require(frame_ < kMaxFrames, TESSERA_BUDGET_EXCEEDED, TESSERA_PHASE_STEP);
    require(apply_input(*system_, inputs), TESSERA_INPUT_CONTRADICTORY,
            TESSERA_PHASE_STEP);
    input_ = inputs;
    // RF-3.8: a mask requested between frames is active from this frame on; the
    // audio event transport only runs while its subscription is.
    subscriptions_.begin_frame();
    system_->set_audio_events(
        subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS));
    system_->set_sprite_capture(
        subscriptions_.active(TESSERA_OBS_SUB_SPRITE_CAPTURE));
    system_->set_attribution(
        subscriptions_.active(TESSERA_OBS_SUB_ATTRIBUTION));
    system_->run_frame();
    ++frame_;
    capture_output();
  }

  [[nodiscard]] Bytes machine_record() const override {
    Bytes record;
    wire::field(record, 1, TESSERA_ENCODING_BYTES, system_->save_state());
    return record;
  }

  void restore_record(std::span<const std::uint8_t> encoded,
                      std::uint64_t frame, const InputState &inputs) override {
    require(frame <= kMaxFrames && pad_mask(inputs, 0) && pad_mask(inputs, 1),
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    const auto fields = wire::record(encoded, TESSERA_CHECKPOINT_INVALID,
                                     TESSERA_PHASE_RESTORE);
    require(fields.size() == 1, TESSERA_CHECKPOINT_INVALID,
            TESSERA_PHASE_RESTORE);
    const auto state =
        wire::get(fields, 0, 1, TESSERA_ENCODING_BYTES,
                  std::numeric_limits<std::size_t>::max(),
                  TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    // load_state leaves the machine as it was when it refuses (RF-4.5).
    require(system_->load_state(state) && system_->frame() == frame,
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    apply_input(*system_, inputs);
    input_ = inputs;
    frame_ = frame;
    pixels_.clear();
    pcm_.clear();
    refresh_frame_geometry();
  }

  [[nodiscard]] std::vector<RegionData>
  regions(std::uint64_t mask) const override {
    require(mask == 0, TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_READ);
    return {};
  }

  [[nodiscard]] IObservable *observation() noexcept override { return this; }

  bool set_subscriptions(std::uint32_t mask) override {
    return subscriptions_.request(mask);
  }
  [[nodiscard]] Subscriptions subscriptions() const override {
    const auto s = subscriptions_.snapshot();
    return {s.supported, s.active, s.requested, s.activation_frame};
  }
  [[nodiscard]] std::int32_t read_region(std::uint32_t id, std::uint32_t offset,
                                         std::span<std::uint8_t> out) override {
    const auto gate = observation::region_gate(id);
    if (gate != 0 && !subscriptions_.active(gate))
      return TESSERA_NOT_SUBSCRIBED; // idle: no work at all
    const auto content = memory_region(*system_, id);
    if (content.empty() && !variable_region(*system_, id))
      return TESSERA_UNSUPPORTED_CAPABILITY; // not served yet (TP-202 onwards)
    if (offset > content.size() || out.size() > content.size() - offset)
      return TESSERA_INVALID_ARGUMENT;
    std::copy_n(content.begin() + offset, out.size(), out.begin());
    work_ += out.size();
    return TESSERA_OK;
  }
  [[nodiscard]] std::uint32_t
  region_byte_size(std::uint32_t id) const override {
    return variable_region(*system_, id) ? line_region_size(*system_, id)
                                         : UINT32_MAX;
  }
  [[nodiscard]] std::int32_t poll_audio_events(std::span<std::uint8_t> out,
                                               std::uint32_t capacity,
                                               std::uint32_t event_size,
                                               std::uint32_t &count) override {
    count = 0;
    if (!subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS))
      return TESSERA_NOT_SUBSCRIBED;
    if (event_size != sizeof(AudioEvent) ||
        out.size() < std::size_t{capacity} * sizeof(AudioEvent))
      return TESSERA_INVALID_ARGUMENT;
    std::vector<AudioEvent> events(capacity);
    count = static_cast<std::uint32_t>(system_->poll_audio_events(events));
    std::memcpy(out.data(), events.data(),
                std::size_t{count} * sizeof(AudioEvent));
    work_ += std::uint64_t{count} * sizeof(AudioEvent);
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  audio_transport_stats(std::span<std::uint8_t> out) const override {
    const auto stats = system_->audio_transport_stats();
    if (out.size() < sizeof(stats))
      return TESSERA_INVALID_ARGUMENT;
    std::memcpy(out.data(), &stats, sizeof(stats));
    return TESSERA_OK;
  }
  [[nodiscard]] std::uint64_t observation_work() const noexcept override {
    return work_;
  }

private:
  void refresh_frame_geometry() {
    const auto size = system_->renderer().size();
    width_ = size.width;
    height_ = size.height;
  }
  void capture_output() {
    refresh_frame_geometry();
    const auto &frame = system_->renderer().frame();
    pixels_.resize(frame.size() * sizeof(frame[0]));
    std::memcpy(pixels_.data(), frame.data(), pixels_.size());
    const auto audio = system_->audio();
    pcm_.resize(audio.size() * sizeof(audio[0]));
    std::memcpy(pcm_.data(), audio.data(), pcm_.size());
  }

  std::unique_ptr<System> system_;
  Standard standard_ = Standard::ntsc;
  InputState input_{};
  Bytes pixels_, pcm_;
  std::uint32_t width_ = 0, height_ = 0;
  std::uint64_t frame_ = 0;
  observation::Subscriptions subscriptions_;
  std::uint64_t work_ = 0; ///< bytes the observation delivered (TP-201.3)
};
} // namespace

std::unique_ptr<IEmulatedSystem> make_backend(std::string_view id,
                                              wire::View content,
                                              wire::View memory,
                                              RegionChoice choice) {
  require(id == "megadrive", TESSERA_IMPLEMENTATION_UNAVAILABLE,
          TESSERA_PHASE_CREATE);
  return std::make_unique<NativeBackend>(content, memory, choice);
}

std::unique_ptr<IEmulatedSystem>
make_backend(std::string_view id, wire::View content, wire::View memory) {
  return make_backend(id, content, memory, RegionChoice::automatic);
}

} // namespace tessera::md
