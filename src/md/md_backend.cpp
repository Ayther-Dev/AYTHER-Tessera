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

class NativeBackend final : public IEmulatedSystem, public IObservable {
public:
  NativeBackend(wire::View content, wire::View memory, RegionChoice choice) {
    // RNF-5: the 16 MiB published by limits_query_v1 (max_rom_bytes), plus a
    // copier header, as load_cart accepts (TP-208.3).
    require(content.size() >= 512 && content.size() % 2 == 0 &&
                content.size() <= kMaxRomSize + kCopierHeader,
            TESSERA_INVALID_CONTENT, TESSERA_PHASE_CREATE);
    require(memory.size() == 65536 &&
                std::all_of(memory.begin(), memory.end(),
                            [](std::uint8_t byte) { return byte == 0; }),
            TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_CREATE);
    // The machine runs the plain image: the content itself, or the one
    // unwrapped from a copier header or .smd; more than 16 MiB, or no SEGA
    // header, is INVALID_CONTENT (TP-208.3).
    std::vector<std::uint8_t> rom = load_cart(content).rom;
    // TP-108.4: the header chooses the console unless the caller fixed it.
    auto config = region_config(choice, rom);
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
    apply_controls();
    // TP-205.4: the dirty patterns are gathered while VDP_MEMORY or
    // RASTER_TRACKING is active; the first frame they are again, everything
    // is dirty (nothing was followed before it).
    const bool tracking =
        subscriptions_.active(TESSERA_OBS_SUB_VDP_MEMORY) ||
        subscriptions_.active(TESSERA_OBS_SUB_RASTER_TRACKING);
    if (tracking && !delta_tracking_)
      delta_all_dirty_ = true;
    delta_tracking_ = tracking;
    system_->run_frame();
    ++frame_;
    capture_output();
    freeze_delta();
    update_screen();
    if (subscriptions_.active(TESSERA_OBS_SUB_FRAME_HASH))
      hash_frame();
  }

  [[nodiscard]] Bytes machine_record() const override {
    Bytes record;
    wire::field(record, 1, TESSERA_ENCODING_BYTES, system_->save_state());
    // RF-3.9: the written controls are part of the state.
    const Bytes controls{layer_mask_, layer_dim_};
    wire::field(record, 2, TESSERA_ENCODING_BYTES, controls);
    return record;
  }

  void restore_record(std::span<const std::uint8_t> encoded,
                      std::uint64_t frame, const InputState &inputs) override {
    require(frame <= kMaxFrames && pad_mask(inputs, 0) && pad_mask(inputs, 1),
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    const auto fields = wire::record(encoded, TESSERA_CHECKPOINT_INVALID,
                                     TESSERA_PHASE_RESTORE);
    require(fields.size() == 2, TESSERA_CHECKPOINT_INVALID,
            TESSERA_PHASE_RESTORE);
    const auto state =
        wire::get(fields, 0, 1, TESSERA_ENCODING_BYTES,
                  std::numeric_limits<std::size_t>::max(),
                  TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    const auto controls =
        wire::get(fields, 1, 2, TESSERA_ENCODING_BYTES, 2,
                  TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    require(controls.size() == 2 &&
                valid_control(kLayerMaskRegion, controls[0]) &&
                valid_control(kLayerDimRegion, controls[1]),
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    // load_state leaves the machine as it was when it refuses (RF-4.5).
    require(system_->load_state(state) && system_->frame() == frame,
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    apply_input(*system_, inputs);
    input_ = inputs;
    frame_ = frame;
    layer_mask_ = controls[0];
    layer_dim_ = controls[1];
    apply_controls();
    // A restore empties the recomposition caches; the counters go on (gpgx).
    single_cache_.valid = multilayer_cache_.valid = false;
    delta_restored();
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
  void keep_observation(IEmulatedSystem &previous) override {
    auto *old = dynamic_cast<NativeBackend *>(&previous);
    if (old == nullptr)
      return;
    subscriptions_ = old->subscriptions_;
    audio_mute_ = old->audio_mute_; // outside the state: a restore keeps it
    stats_ = old->stats_;           // the caches start empty
    // The frame delta history and its generation are not machine state
    // either: gpgx keeps counting frames through retro_unserialize.
    delta_ring_ = old->delta_ring_;
    delta_generation_ = old->delta_generation_;
    delta_tracking_ = old->delta_tracking_;
    delta_restored();
    // FRAME_HASH stays as the last frame left it, and the screen it walks is
    // the frontend's bitmap, not machine state (measured on gpgx).
    frame_hash_ = old->frame_hash_;
    screen_ = old->screen_;
    screen_width_ = old->screen_width_;
    screen_height_ = old->screen_height_;
    hash_width_ = old->hash_width_;
    hash_height_ = old->hash_height_;
    apply_controls();
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
      return TESSERA_NOT_SUBSCRIBED; // idle: no work at all
    if (id == TESSERA_OBS_REGION_FRAME_HASH) {
      if (offset > frame_hash_.size() ||
          out.size() > frame_hash_.size() - offset)
        return TESSERA_INVALID_ARGUMENT;
      std::copy_n(frame_hash_.begin() + offset, out.size(), out.begin());
      work_ += out.size();
      return TESSERA_OK;
    }
    if (id == kLayerMaskRegion || id == kLayerDimRegion) {
      // Read back as written, with or without RENDER_CONTROLS (measured).
      if (offset != 0 || out.size() > 1)
        return TESSERA_INVALID_ARGUMENT;
      if (!out.empty())
        out[0] = id == kLayerMaskRegion ? layer_mask_ : layer_dim_;
      work_ += out.size();
      return TESSERA_OK;
    }
    if (id == kAudioMuteRegion) {
      // Read back as written, like the layer controls.
      if (offset > sizeof(audio_mute_) ||
          out.size() > sizeof(audio_mute_) - offset)
        return TESSERA_INVALID_ARGUMENT;
      std::memcpy(out.data(),
                  reinterpret_cast<const std::uint8_t *>(&audio_mute_) + offset,
                  out.size());
      work_ += out.size();
      return TESSERA_OK;
    }
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
  [[nodiscard]] std::int32_t recompose(std::span<std::uint16_t> out,
                                       std::uint32_t flags,
                                       std::uint32_t &width,
                                       std::uint32_t &height) override {
    width = height = 0;
    if (!subscriptions_.active(TESSERA_OBS_SUB_RECOMPOSITION))
      return TESSERA_NOT_SUBSCRIBED;
    FrameSize size;
    const auto frame = system_->renderer().recompose(system_->vdp(), size);
    // gpgx refuses interlace mode 2 (AYTHER_STATUS_RC_INTERLACE2) and a
    // buffer smaller than the frame (RC_INVALID_PARAMS).
    if (frame.empty())
      return TESSERA_WRONG_STATE;
    if (out.size() < frame.size())
      return TESSERA_INVALID_ARGUMENT;
    std::transform(frame.begin(), frame.end(), out.begin(), rgba_to_565);
    width = size.width;
    height = size.height;
    work_ += frame.size() * sizeof(std::uint16_t);
    count(single_cache_, flags, stats_.single_calls, stats_.single_hits);
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  recompose_layers(std::array<std::span<std::uint16_t>, 5> out,
                   std::uint32_t flags, std::uint32_t &width,
                   std::uint32_t &height) override {
    width = height = 0;
    if (!subscriptions_.active(TESSERA_OBS_SUB_RECOMPOSITION))
      return TESSERA_NOT_SUBSCRIBED;
    // With RENDER_CONTROLS every layer is drawn alone (measured on gpgx).
    std::array<std::vector<std::uint32_t>, 5> layers;
    FrameSize size;
    if (!system_->renderer().recompose_layers(
            system_->vdp(),
            subscriptions_.active(TESSERA_OBS_SUB_RENDER_CONTROLS),
            subscriptions_.active(TESSERA_OBS_SUB_RASTER_TRACKING), layers,
            size))
      return TESSERA_WRONG_STATE; // gpgx RC_NOT_MODE5, RC_INTERLACE2,
                                  // RC_JOURNAL_OVERFLOW
    for (unsigned i = 0; i < 5; ++i)
      if (out[i].size() < layers[i].size())
        return TESSERA_INVALID_ARGUMENT;
    for (unsigned i = 0; i < 5; ++i)
      std::transform(layers[i].begin(), layers[i].end(), out[i].begin(),
                     rgba_to_565);
    width = size.width;
    height = size.height;
    work_ += 5U * layers[0].size() * sizeof(std::uint16_t);
    count(multilayer_cache_, flags, stats_.multilayer_calls,
          stats_.multilayer_hits);
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  recompose_stats(RecomposeStats &out) const override {
    out = stats_;
    out.controls_fingerprint = controls_fingerprint();
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t frame_delta(FrameDelta &out) const override {
    out = latest_delta();
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t frame_delta_since(std::uint64_t from,
                                               FrameDelta &out) const override {
    out = latest_delta();
    if (delta_generation_ == 0)
      return TESSERA_OK;
    // Every frame after `from`; a generation not before the last one gives the
    // last frame (measured on gpgx). Eight frames are kept.
    const std::uint64_t first =
        std::min(from, delta_generation_ - 1) + 1; // exclusive of `from`
    if (delta_generation_ - first + 1 > kDeltaHistory) {
      out.dirty.fill(0xFF);
      return TESSERA_STALE_EPOCH; // gpgx AYTHER_STATUS_DELTA_HISTORY_LOST
    }
    for (std::uint64_t g = first; g < delta_generation_; ++g) {
      const auto &older = delta_ring_[g % kDeltaHistory].dirty;
      for (std::size_t i = 0; i < out.dirty.size(); ++i)
        out.dirty[i] |= older[i];
    }
    return TESSERA_OK;
  }
  [[nodiscard]] std::int32_t
  write_control(std::uint32_t id, std::uint32_t offset,
                std::span<const std::uint8_t> data) override {
    // Work RAM poke (RF-3.1): region bytes, word-swapped like the read; the
    // RAM is machine state, so the savestate and checkpoints carry it.
    if (id == kRegionWorkRam)
      return poke_memory_region(*system_, id, offset, data)
                 ? TESSERA_OK
                 : TESSERA_INVALID_ARGUMENT;
    if (id == kAudioMuteRegion) {
      // gpgx: a u32 at offset 0 with bits 0-17 (FM 0-5, PSG 6-9, PCM 10-17);
      // another size or a higher bit is -1, another offset -4 (measured).
      std::uint32_t mask = 0;
      if (offset != 0 || data.size() != sizeof(mask))
        return TESSERA_INVALID_ARGUMENT;
      std::memcpy(&mask, data.data(), sizeof(mask));
      if ((mask & ~System::kAudioMuteBits) != 0)
        return TESSERA_INVALID_ARGUMENT;
      audio_mute_ = mask;
      apply_controls();
      return TESSERA_OK;
    }
    if (id != kLayerMaskRegion && id != kLayerDimRegion)
      return TESSERA_UNSUPPORTED_CAPABILITY;
    if (offset != 0 || data.size() != 1 || !valid_control(id, data[0]) ||
        data[0] == 0xFF)
      return TESSERA_INVALID_ARGUMENT;
    (id == kLayerMaskRegion ? layer_mask_ : layer_dim_) = data[0];
    apply_controls();
    return TESSERA_OK;
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
  static constexpr std::uint32_t kLayerMaskRegion =
      TESSERA_OBS_REGION_LAYER_MASK;
  static constexpr std::uint32_t kLayerDimRegion = TESSERA_OBS_REGION_LAYER_DIM;
  static constexpr std::uint32_t kAudioMuteRegion =
      TESSERA_OBS_REGION_AUDIO_MUTE;
  /// gpgx refuses a mask beyond the four layers and a dim other than 0 or 1;
  /// 0xFF, the mask before any write, is valid only as restored state.
  static bool valid_control(std::uint32_t id, std::uint8_t value) noexcept {
    return id == kLayerMaskRegion ? value <= 0x0F || value == 0xFF : value <= 1;
  }
  /// The controls act only while RENDER_CONTROLS is active (measured).
  void apply_controls() {
    const bool on = subscriptions_.active(TESSERA_OBS_SUB_RENDER_CONTROLS);
    system_->set_render_controls(
        on ? static_cast<std::uint8_t>(layer_mask_ & Renderer::kLayerAll)
           : Renderer::kLayerAll,
        on && layer_dim_ != 0);
    // AUDIO_MUTE too acts only while RENDER_CONTROLS is (measured on gpgx).
    // It is an output control, not machine state (savestate-v1.md).
    system_->set_audio_mute(on ? audio_mute_ : 0U);
  }
  /// gpgx's recomposition cache: one entry per kind, keyed on the frame, the
  /// flags and the controls (TP-205.3). Only accepted calls count.
  struct RecomposeCache {
    bool valid = false;
    std::uint64_t frame = 0, fingerprint = 0;
    std::uint32_t flags = 0;
  };
  void count(RecomposeCache &cache, std::uint32_t flags, std::uint64_t &calls,
             std::uint64_t &hits) const {
    const auto fingerprint = controls_fingerprint();
    ++calls;
    if (cache.valid && cache.frame == frame_ && cache.flags == flags &&
        cache.fingerprint == fingerprint)
      ++hits;
    cache = {true, frame_, fingerprint, flags};
  }
  /// FNV-1a 64 over what gpgx hashes, as measured: RENDER_CONTROLS active,
  /// LAYER_MASK and LAYER_DIM as written, then the suppression state (a flag
  /// and 16 + 512 + 3072 bytes), which Tessera does not take (PQ-1): zeros.
  [[nodiscard]] std::uint64_t controls_fingerprint() const {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    const auto mix = [&hash](std::uint8_t byte) {
      hash = (hash ^ byte) * 0x100000001b3ULL;
    };
    mix(subscriptions_.active(TESSERA_OBS_SUB_RENDER_CONTROLS) ? 1 : 0);
    mix(layer_mask_);
    mix(layer_dim_);
    for (unsigned i = 0; i < 1U + 16U + 512U + 3072U; ++i)
      mix(0);
    return hash;
  }
  static constexpr std::uint64_t kDeltaHistory = 8;
  /// The delta of the frame just run, frozen into the history (TP-205.4).
  /// Each counter is the one of its subscription, 0 while it is idle.
  void freeze_delta() {
    const auto dirty = system_->take_vram_dirty();
    FrameDelta delta{};
    delta.generation = ++delta_generation_;
    if (delta_all_dirty_)
      delta.dirty.fill(0xFF);
    else if (delta_tracking_)
      delta.dirty = dirty;
    delta_all_dirty_ = false;
    if (subscriptions_.active(TESSERA_OBS_SUB_RASTER_TRACKING)) {
      delta.raster_events = system_->vdp().raster().count;
      delta.raster_dropped = system_->vdp().raster().dropped;
      // The frame snapshot's fallback_reasons: RASTER_FALLBACK_REASONS as
      // this frame left it (measured on gpgx: equal to region 16).
      delta.fallback = system_->vdp().raster().fallback |
                       (double_resolution(*system_) ? 0x20U : 0U);
    }
    // The parsed sprites never overflow: the table has 80 entries and the
    // region 128 (gpgx never set bit 0 over the sprite_limits ROMs).
    if (subscriptions_.active(TESSERA_OBS_SUB_SPRITE_CAPTURE))
      delta.parsed_sprites = system_->renderer().sprite_capture().count;
    if (subscriptions_.active(TESSERA_OBS_SUB_AUDIO_WRITES)) {
      delta.audio_writes =
          static_cast<std::uint32_t>(system_->audio_writes().size());
      // gpgx raises AYTHER_OVERFLOW_AUDIO_WRITES once the frame dropped
      // writes past the 8 192 kept (audio_flood).
      if (system_->audio_writes_dropped() != 0)
        delta.overflow |= TESSERA_OBS_OVERFLOW_AUDIO_WRITES;
    }
    delta_ring_[delta.generation % kDeltaHistory] = delta;
  }
  /// A restore may replace the whole VRAM: the last frame's delta and the
  /// next one report everything dirty, and the frozen counters read 0
  /// (measured on gpgx through retro_unserialize).
  void delta_restored() {
    delta_all_dirty_ = true;
    if (delta_generation_ == 0)
      return;
    auto &last = delta_ring_[delta_generation_ % kDeltaHistory];
    last = {};
    last.generation = delta_generation_;
    last.dirty.fill(0xFF);
  }
  [[nodiscard]] FrameDelta latest_delta() const {
    if (delta_generation_ != 0)
      return delta_ring_[delta_generation_ % kDeltaHistory];
    FrameDelta none{}; // before the first frame: nothing is known yet
    none.dirty.fill(0xFF);
    return none;
  }
  /// gpgx draws into a 720-pixel-wide bitmap that is never cleared: what a
  /// smaller frame does not cover keeps older pixels. Kept here as RGB565.
  static constexpr std::uint32_t kScreenWidth = 720, kScreenHeight = 576;
  void update_screen() {
    const auto &frame = system_->renderer().frame();
    const auto w = std::min(width_, kScreenWidth);
    const auto h = std::min(height_, kScreenHeight);
    if (frame.size() < std::size_t{width_} * height_)
      return;
    for (std::uint32_t y = 0; y < h; ++y)
      std::transform(frame.begin() + std::size_t{y} * width_,
                     frame.begin() + std::size_t{y} * width_ + w,
                     screen_.begin() + std::size_t{y} * kScreenWidth,
                     rgba_to_565);
    // FRAME_HASH walks the geometry of the frame before (gpgx updates its
    // viewport after hashing; measured on modes_sweep 0 to 33).
    hash_width_ = screen_width_ != 0 ? screen_width_ : w;
    hash_height_ = screen_height_ != 0 ? screen_height_ : h;
    screen_width_ = w;
    screen_height_ = h;
  }
  /// FRAME_HASH (TP-206.1): FNV-1a 64 with gpgx's offset basis
  /// 1469598103934665603 over the emitted pixels (RGB565 little endian, row
  /// by row), the frame's samples, and the VRAM, CRAM and VSRAM regions as
  /// they are read; frame_index is the frame just run, counted from 0.
  void hash_frame() {
    constexpr std::uint64_t kBasis = 1469598103934665603ULL;
    const auto fnv = [](std::uint64_t hash, const std::uint8_t *p,
                        std::size_t n) {
      for (std::size_t i = 0; i < n; ++i)
        hash = (hash ^ p[i]) * 0x100000001b3ULL;
      return hash;
    };
    std::uint64_t video = kBasis;
    for (std::uint32_t y = 0; y < hash_height_; ++y)
      for (std::uint32_t x = 0; x < hash_width_; ++x) {
        const std::uint16_t px = screen_[std::size_t{y} * kScreenWidth + x];
        const std::uint8_t bytes[2] = {static_cast<std::uint8_t>(px),
                                       static_cast<std::uint8_t>(px >> 8)};
        video = fnv(video, bytes, 2);
      }
    const auto memory = [&](std::uint32_t id) {
      const auto bytes = memory_region(*system_, id);
      return fnv(kBasis, bytes.data(), bytes.size());
    };
    const std::uint64_t fields[] = {delta_generation_ - 1,
                                    video,
                                    fnv(kBasis, pcm_.data(), pcm_.size()),
                                    memory(TESSERA_OBS_REGION_VRAM),
                                    memory(TESSERA_OBS_REGION_CRAM),
                                    memory(TESSERA_OBS_REGION_VSRAM)};
    const std::uint32_t header[] = {1, 56};
    std::memcpy(frame_hash_.data(), header, sizeof(header));
    std::memcpy(frame_hash_.data() + 8, fields, sizeof(fields));
  }
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
  std::uint8_t layer_mask_ = 0xFF; ///< LAYER_MASK as written (gpgx: 0xFF)
  std::uint8_t layer_dim_ = 0;     ///< LAYER_DIM as written
  std::uint32_t audio_mute_ = 0;   ///< AUDIO_MUTE as written (not in the state)
  std::uint64_t work_ = 0; ///< bytes the observation delivered (TP-201.3)
  RecomposeStats stats_{};
  RecomposeCache single_cache_, multilayer_cache_;
  std::array<FrameDelta, kDeltaHistory> delta_ring_{};
  std::uint64_t delta_generation_ = 0; ///< frames run (gpgx's generation)
  bool delta_tracking_ = false;
  bool delta_all_dirty_ = true; ///< power-on: the first frame is all dirty
  std::array<std::uint8_t, 56> frame_hash_{}; ///< tessera_obs_frame_hash_v1
  std::vector<std::uint16_t> screen_ =
      std::vector<std::uint16_t>(std::size_t{kScreenWidth} * kScreenHeight);
  std::uint32_t screen_width_ = 0, screen_height_ = 0;
  std::uint32_t hash_width_ = 0, hash_height_ = 0;
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
