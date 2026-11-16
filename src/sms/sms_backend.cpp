#include "sms/sms_backend.hpp"

#include "domain/result.hpp"
#include "domain/wire.hpp"
#include "md/audio_events.hpp"
#include "observation/region_table.hpp"
#include "observation/subscriptions.hpp"
#include "sms/system.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace tessera::sms {
namespace {

constexpr std::uint64_t kCapabilities = 0x20F;
constexpr std::uint64_t kMaxFrames = 1ULL << 40;

std::uint16_t argb_to_rgb565(std::uint32_t c) noexcept {
  const std::uint16_t r = static_cast<std::uint16_t>((c >> 19) & 0x1FU);
  const std::uint16_t g = static_cast<std::uint16_t>((c >> 10) & 0x3FU);
  const std::uint16_t b = static_cast<std::uint16_t>((c >> 3) & 0x1FU);
  return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
}

std::uint16_t sms_pad_mask(const InputState &input, unsigned port) noexcept {
  const auto *c = input.data() + std::size_t{port} * kPadControls;
  std::uint16_t mask = 0;
  if (c[0])
    mask |= kSmsUp;
  if (c[1])
    mask |= kSmsDown;
  if (c[2])
    mask |= kSmsLeft;
  if (c[3])
    mask |= kSmsRight;
  if (c[4] || c[5])
    mask |= kSmsButton1;
  if (c[6])
    mask |= kSmsButton2;
  if (c[7])
    mask |= kSmsPauseOrStart;
  return mask;
}

class SmsBackend final : public IEmulatedSystem, public IObservable {
public:
  SmsBackend(std::string_view id, wire::View content, wire::View memory)
      : id_(id) {
    require(!content.empty() && content.size() <= 4U * 1024U * 1024U,
            TESSERA_INVALID_CONTENT, TESSERA_PHASE_CREATE);
    require(memory.size() == 65536 &&
                std::all_of(memory.begin(), memory.end(),
                            [](std::uint8_t byte) { return byte == 0; }),
            TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_CREATE);

    if (id == "gamegear") {
      model_ = ConsoleModel::game_gear;
      sys_id_ = TESSERA_SYSTEM_GAME_GEAR;
    } else if (id == "sg1000") {
      model_ = ConsoleModel::sg1000;
      sys_id_ = TESSERA_SYSTEM_SG1000;
    } else {
      model_ = ConsoleModel::sms;
      sys_id_ = TESSERA_SYSTEM_MASTER_SYSTEM;
    }

    std::vector<std::uint8_t> rom(content.begin(), content.end());
    if (rom.size() >= 512 && (rom.size() % 1024U) == 512U) {
      rom.erase(rom.begin(), rom.begin() + 512);
    }

    system_ = std::make_unique<System>(std::move(rom), model_);
    system_->reset();
    width_ = system_->vdp().width();
    height_ = system_->vdp().height();
  }

  [[nodiscard]] std::string_view backend_id() const noexcept override {
    return id_;
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
  [[nodiscard]] std::uint32_t system() const noexcept override {
    return sys_id_;
  }
  [[nodiscard]] std::uint32_t frame_width() const noexcept override {
    return width_;
  }
  [[nodiscard]] std::uint32_t frame_height() const noexcept override {
    return height_;
  }
  [[nodiscard]] std::uint64_t frame_ticks() const noexcept override {
    return 59748ULL;
  }
  [[nodiscard]] std::uint64_t tick_hz() const noexcept override {
    return 3579545ULL;
  }

  void step(const InputState &inputs) override {
    require(frame_ < kMaxFrames, TESSERA_BUDGET_EXCEEDED, TESSERA_PHASE_STEP);
    const auto *c0 = inputs.data();
    const auto *c1 = inputs.data() + kPadControls;
    require(!((c0[0] && c0[1]) || (c0[2] && c0[3]) || (c1[0] && c1[1]) ||
              (c1[2] && c1[3])),
            TESSERA_INPUT_CONTRADICTORY, TESSERA_PHASE_STEP);
    system_->set_pad(0, sms_pad_mask(inputs, 0));
    system_->set_pad(1, sms_pad_mask(inputs, 1));
    input_ = inputs;
    subscriptions_.begin_frame();
    system_->set_audio_mute(audio_mute_);
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
    require(frame <= kMaxFrames, TESSERA_CHECKPOINT_INVALID,
            TESSERA_PHASE_RESTORE);
    const auto fields = wire::record(encoded, TESSERA_CHECKPOINT_INVALID,
                                     TESSERA_PHASE_RESTORE);
    const auto state =
        wire::get(fields, 1, 1, TESSERA_ENCODING_BYTES,
                  std::numeric_limits<std::size_t>::max(),
                  TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    require(system_->load_state(state) && system_->frame() == frame,
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    system_->set_pad(0, sms_pad_mask(inputs, 0));
    system_->set_pad(1, sms_pad_mask(inputs, 1));
    input_ = inputs;
    frame_ = frame;
    pixels_.clear();
    pcm_.clear();
  }

  [[nodiscard]] std::vector<RegionData>
  regions(std::uint64_t mask) const override {
    require(mask == 0, TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_READ);
    return {};
  }

  [[nodiscard]] IObservable *observation() noexcept override { return this; }
  void keep_observation(IEmulatedSystem &previous) override {
    auto *old = dynamic_cast<SmsBackend *>(&previous);
    if (old == nullptr)
      return;
    subscriptions_ = old->subscriptions_;
    audio_mute_ = old->audio_mute_;
    system_->set_audio_mute(audio_mute_);
  }

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
      return TESSERA_NOT_SUBSCRIBED;
    if (id == TESSERA_OBS_REGION_AUDIO_MUTE) {
      if (offset > sizeof(audio_mute_) ||
          out.size() > sizeof(audio_mute_) - offset)
        return TESSERA_INVALID_ARGUMENT;
      std::memcpy(out.data(),
                  reinterpret_cast<const std::uint8_t *>(&audio_mute_) + offset,
                  out.size());
      work_ += out.size();
      return TESSERA_OK;
    }
    std::span<const std::uint8_t> src;
    if (id == TESSERA_OBS_REGION_VRAM)
      src = system_->vdp().vram();
    else if (id == TESSERA_OBS_REGION_CRAM)
      src = system_->vdp().cram();
    else if (id == TESSERA_OBS_REGION_WORK_RAM)
      src = system_->ram();
    else
      return TESSERA_UNSUPPORTED_CAPABILITY;
    if (offset > src.size() || out.size() > src.size() - offset)
      return TESSERA_INVALID_ARGUMENT;
    std::copy_n(src.begin() + offset, out.size(), out.begin());
    work_ += out.size();
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  write_control(std::uint32_t id, std::uint32_t offset,
                std::span<const std::uint8_t> value) override {
    if (id == TESSERA_OBS_REGION_AUDIO_MUTE) {
      if (offset != 0 || value.size() != sizeof(audio_mute_))
        return TESSERA_INVALID_ARGUMENT;
      std::memcpy(&audio_mute_, value.data(), sizeof(audio_mute_));
      audio_mute_ &= System::kAudioMuteBits;
      system_->set_audio_mute(audio_mute_);
      return TESSERA_OK;
    }
    return TESSERA_UNSUPPORTED_CAPABILITY;
  }

  [[nodiscard]] std::int32_t poll_audio_events(std::span<std::uint8_t> /*out*/,
                                               std::uint32_t /*capacity*/,
                                               std::uint32_t event_size,
                                               std::uint32_t &count) override {
    count = 0;
    if (event_size != sizeof(md::AudioEvent))
      return TESSERA_INVALID_ARGUMENT;
    if (!subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS))
      return TESSERA_NOT_SUBSCRIBED;
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  audio_transport_stats(std::span<std::uint8_t> out) const override {
    const auto stats = md::AudioEventQueue::idle_stats(
        subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS)
            ? md::kAudioTransportObservationActive
            : 0U);
    if (out.size() != sizeof(stats))
      return TESSERA_INVALID_ARGUMENT;
    std::memcpy(out.data(), &stats, sizeof(stats));
    return TESSERA_OK;
  }
  [[nodiscard]] std::uint64_t observation_work() const noexcept override {
    return work_;
  }

private:
  void capture_output() {
    const auto fb = system_->vdp().framebuffer();
    pixels_.resize(std::size_t{width_} * height_ * sizeof(std::uint16_t));
    auto *dst = reinterpret_cast<std::uint16_t *>(pixels_.data());
    const int x_off = model_ == ConsoleModel::game_gear ? 48 : 0;
    const int y_off = model_ == ConsoleModel::game_gear ? 24 : 0;
    for (std::uint32_t y = 0; y < height_; ++y) {
      for (std::uint32_t x = 0; x < width_; ++x) {
        const std::uint32_t c =
            fb[std::size_t{y + y_off} * VdpM4::kSmsWidth + (x + x_off)];
        dst[std::size_t{y} * width_ + x] = argb_to_rgb565(c);
      }
    }
    const auto audio = system_->audio();
    pcm_.resize(audio.size() * sizeof(audio[0]));
    std::memcpy(pcm_.data(), audio.data(), pcm_.size());
  }

  std::string_view id_;
  ConsoleModel model_ = ConsoleModel::sms;
  std::uint32_t sys_id_ = TESSERA_SYSTEM_MASTER_SYSTEM;
  std::unique_ptr<System> system_;
  InputState input_{};
  Bytes pixels_, pcm_;
  std::uint32_t width_ = 256;
  std::uint32_t height_ = 192;
  std::uint64_t frame_ = 0;
  std::uint32_t audio_mute_ = 0;
  std::uint64_t work_ = 0;
  observation::Subscriptions subscriptions_;
};

} // namespace

std::unique_ptr<IEmulatedSystem>
make_backend(std::string_view id, wire::View content, wire::View memory) {
  return std::make_unique<SmsBackend>(id, content, memory);
}

} // namespace tessera::sms
