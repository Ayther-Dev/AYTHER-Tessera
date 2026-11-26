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
    require(memory.size() <= 65536, TESSERA_UNSUPPORTED_CAPABILITY,
            TESSERA_PHASE_CREATE);

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
    system_->set_observe_audio(
        subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS) ||
        subscriptions_.active(TESSERA_OBS_SUB_AUDIO_WRITES));
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
    std::size_t abi_size = 0;
    std::array<std::uint8_t, 44> sys_buf{};
    std::array<std::uint8_t, 64> cram_buf{};
    std::vector<std::uint8_t> audio_buf;
    if (id == TESSERA_OBS_REGION_VRAM) {
      src = system_->vdp().vram();
      abi_size = 65536;
    } else if (id == TESSERA_OBS_REGION_CRAM) {
      const auto raw = system_->vdp().cram();
      if (model_ == ConsoleModel::game_gear) {
        src = raw;
      } else {
        for (std::size_t i = 0; i < 32 && i < raw.size(); ++i)
          cram_buf[i * 2] = raw[i];
        src = cram_buf;
      }
      abi_size = 128;
    } else if (id == TESSERA_OBS_REGION_VDP_REGS) {
      src = system_->vdp().regs();
      abi_size = 32;
    } else if (id == TESSERA_OBS_REGION_WORK_RAM) {
      src = system_->ram();
      abi_size = 65536;
    } else if (id == TESSERA_OBS_REGION_AUDIO_WRITES) {
      const auto writes = system_->audio_writes();
      audio_buf.resize(writes.size() * 8U);
      for (std::size_t i = 0; i < writes.size(); ++i) {
        const auto &w = writes[i];
        const std::size_t at = i * 8U;
        audio_buf[at + 0] = static_cast<std::uint8_t>(w.cycle);
        audio_buf[at + 1] = static_cast<std::uint8_t>(w.cycle >> 8);
        audio_buf[at + 2] = static_cast<std::uint8_t>(w.cycle >> 16);
        audio_buf[at + 3] = static_cast<std::uint8_t>(w.cycle >> 24);
        audio_buf[at + 4] = static_cast<std::uint8_t>(w.addr);
        audio_buf[at + 5] = static_cast<std::uint8_t>(w.addr >> 8);
        audio_buf[at + 6] = w.data;
        audio_buf[at + 7] = w.chip;
      }
      src = audio_buf;
      abi_size = 65536;
    } else if (id == TESSERA_OBS_REGION_AUDIO_WRITE_COUNT) {
      const std::uint32_t count =
          static_cast<std::uint32_t>(system_->audio_writes().size());
      audio_buf.resize(4);
      audio_buf[0] = static_cast<std::uint8_t>(count);
      audio_buf[1] = static_cast<std::uint8_t>(count >> 8);
      audio_buf[2] = static_cast<std::uint8_t>(count >> 16);
      audio_buf[3] = static_cast<std::uint8_t>(count >> 24);
      src = audio_buf;
      abi_size = 4;
    } else if (id == TESSERA_OBS_REGION_SYSTEM) {
      auto w16 = [&](std::size_t at, std::uint16_t v) {
        sys_buf[at] = static_cast<std::uint8_t>(v);
        sys_buf[at + 1] = static_cast<std::uint8_t>(v >> 8);
      };
      auto w32 = [&](std::size_t at, std::uint32_t v) {
        w16(at, static_cast<std::uint16_t>(v));
        w16(at + 2, static_cast<std::uint16_t>(v >> 16));
      };
      w32(0, 44);
      w32(4, 1);
      sys_buf[8] = model_ == ConsoleModel::game_gear ? 0x40
                   : model_ == ConsoleModel::sg1000  ? 0x10
                                                     : 0x20;
      sys_buf[9] = 0;
      sys_buf[10] = model_ == ConsoleModel::sg1000 ? 0 : 4;
      w16(14, 262);
      w16(20, static_cast<std::uint16_t>(width_));
      w16(22, static_cast<std::uint16_t>(height_));
      w32(24, 3579545U);
      w32(28, 53693175U);
      sys_buf[33] = 1;
      w32(36, system_->rom_crc32());
      src = sys_buf;
      abi_size = sys_buf.size();
    } else {
      return TESSERA_UNSUPPORTED_CAPABILITY;
    }
    if (offset > abi_size || out.size() > abi_size - offset)
      return TESSERA_INVALID_ARGUMENT;
    const std::size_t avail =
        offset < src.size() ? std::min(out.size(), src.size() - offset) : 0;
    if (avail > 0)
      std::copy_n(src.begin() + offset, avail, out.begin());
    if (avail < out.size())
      std::fill(out.begin() + avail, out.end(), std::uint8_t{0});
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

  [[nodiscard]] std::int32_t poll_audio_events(std::span<std::uint8_t> out,
                                               std::uint32_t capacity,
                                               std::uint32_t event_size,
                                               std::uint32_t &count) override {
    count = 0;
    if (event_size != sizeof(md::AudioEvent))
      return TESSERA_INVALID_ARGUMENT;
    if (!subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS))
      return TESSERA_NOT_SUBSCRIBED;
    const std::size_t n =
        std::min<std::size_t>(capacity, out.size() / sizeof(md::AudioEvent));
    if (n == 0)
      return TESSERA_OK;
    count = static_cast<std::uint32_t>(system_->audio_events().poll(
        {reinterpret_cast<md::AudioEvent *>(out.data()), n}));
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  audio_transport_stats(std::span<std::uint8_t> out) const override {
    const auto stats = system_->audio_events().stats(
        subscriptions_.active(TESSERA_OBS_SUB_AUDIO_EVENTS)
            ? md::kAudioTransportObservationActive
            : 0U);
    if (out.size() != sizeof(stats))
      return TESSERA_INVALID_ARGUMENT;
    std::memcpy(out.data(), &stats, sizeof(stats));
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t frame_delta(FrameDelta &out) const override {
    out = {};
    out.generation = frame_;
    out.audio_writes =
        static_cast<std::uint32_t>(system_->audio_writes().size());
    out.dirty.fill(0xFF);
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t frame_delta_since(std::uint64_t /*from*/,
                                               FrameDelta &out) const override {
    return frame_delta(out);
  }
  [[nodiscard]] std::uint64_t observation_work() const noexcept override {
    return work_;
  }

private:
  void capture_output() {
    const auto fb = system_->vdp().framebuffer();
    pixels_.resize(std::size_t{width_} * height_ * 4U);
    auto *dst = pixels_.data();
    const int x_off = model_ == ConsoleModel::game_gear ? 48 : 0;
    const int y_off = model_ == ConsoleModel::game_gear ? 24 : 0;
    for (std::uint32_t y = 0; y < height_; ++y) {
      for (std::uint32_t x = 0; x < width_; ++x) {
        const std::uint32_t c =
            fb[std::size_t{y + y_off} * VdpM4::kSmsWidth + (x + x_off)];
        const std::size_t at = (std::size_t{y} * width_ + x) * 4U;
        dst[at + 0] = static_cast<std::uint8_t>((c >> 16) & 0xFFU);
        dst[at + 1] = static_cast<std::uint8_t>((c >> 8) & 0xFFU);
        dst[at + 2] = static_cast<std::uint8_t>(c & 0xFFU);
        dst[at + 3] = 0xFFU;
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
