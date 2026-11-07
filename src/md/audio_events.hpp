// Audio event transport of the native Mega Drive core (spec 008, TP-107.6).
//
// The record is the public `ayther_audio_event_v1` (layout 2, 80 bytes) and
// the queue a bounded single-producer single-consumer ring with acquire/release
// publication, as the AYTHER ABI describes it: the producer is the emulator,
// the consumer a tool that drains it with `poll`. This file is the transport
// only. The logic that derives anchors (NOTE_ON, DAC_START), groups and the
// resolved FM voice from the writes belongs to the audio probe and is not
// here: the native core emits RAW_WRITE for every sound chip write and FRAME
// at the end of every frame, which is all the transport carries.
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tessera::md {

enum class AudioSource : std::uint8_t { fm = 0, psg = 1, dac = 2, pcm = 3 };
enum class AudioEventType : std::uint8_t {
  raw_write = 0,
  note_on,
  note_off,
  dac_start,
  dac_stop,
  patch,
  pitch,
  volume,
  reset,
  state_load,
  frame
};

inline constexpr std::uint8_t kAudioEventSchema = 2; ///< LAYOUT_AUDIO_EVENT_V1
inline constexpr std::uint8_t kAudioNoChannel = 255;

/// `ayther_audio_event_v1`. Only the {reg, data} arm of the union is used.
struct AudioEvent {
  std::uint64_t t_global; ///< master cycles since the machine started
  std::uint32_t t_frame;  ///< index of the frame the event belongs to
  std::uint32_t t_cycles; ///< master cycles since that frame started
  std::uint8_t source;
  std::uint8_t type;
  std::uint8_t channel;
  std::uint8_t schema;
  std::uint32_t group;
  std::uint32_t reg;
  std::uint32_t data;
  std::array<std::uint8_t, 48> unused; ///< rest of the `voice` arm
};
static_assert(sizeof(AudioEvent) == 80, "ayther_audio_event_v1 is 80 bytes");
static_assert(offsetof(AudioEvent, t_frame) == 8);
static_assert(offsetof(AudioEvent, t_cycles) == 12);
static_assert(offsetof(AudioEvent, source) == 16);
static_assert(offsetof(AudioEvent, group) == 20);
static_assert(offsetof(AudioEvent, reg) == 24);
static_assert(offsetof(AudioEvent, data) == 28);

/// `ayther_audio_transport_stats_v1`.
struct AudioTransportStats {
  std::uint32_t struct_size;
  std::uint32_t transport_version;
  std::uint32_t event_size;
  std::uint32_t capacity;
  std::uint32_t pending;
  std::uint32_t high_water_mark;
  std::uint32_t dropped_events;
  std::uint32_t flags;
};
static_assert(sizeof(AudioTransportStats) == 32);
inline constexpr std::uint32_t kAudioTransportVersion = 1;
/// Set while an observer is subscribed.
inline constexpr std::uint32_t kAudioTransportObservationActive = 1U << 1;

class AudioEventQueue {
public:
  /// 8 192 slots, one reserved by the full/empty protocol: 8 191 events.
  static constexpr std::size_t kSlots = 8192;
  static constexpr std::uint32_t kCapacity = kSlots - 1;

  /// Producer. False (and the dropped counter, which saturates) if full.
  bool push(const AudioEvent &event) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) % kSlots;
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (next == tail) {
      if (dropped_ != UINT32_MAX)
        ++dropped_;
      return false;
    }
    ring_[head] = event;
    head_.store(next, std::memory_order_release); // publish the whole record
    const auto pending = static_cast<std::uint32_t>(
        (next + kSlots - tail_.load(std::memory_order_relaxed)) % kSlots);
    if (pending > high_water_)
      high_water_ = pending;
    return true;
  }

  /// Consumer. Copies up to out.size() events and returns how many.
  std::size_t poll(std::span<AudioEvent> out) noexcept {
    std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    std::size_t count = 0;
    while (tail != head && count < out.size()) {
      out[count++] = ring_[tail];
      tail = (tail + 1) % kSlots;
    }
    tail_.store(tail, std::memory_order_release);
    return count;
  }

  /// The stats of a queue that has never held anything.
  static AudioTransportStats idle_stats(std::uint32_t flags) noexcept {
    return {sizeof(AudioTransportStats),
            kAudioTransportVersion,
            sizeof(AudioEvent),
            kCapacity,
            0,
            0,
            0,
            flags};
  }

  AudioTransportStats stats(std::uint32_t flags) const noexcept {
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    return {sizeof(AudioTransportStats),
            kAudioTransportVersion,
            sizeof(AudioEvent),
            kCapacity,
            static_cast<std::uint32_t>((head + kSlots - tail) % kSlots),
            high_water_,
            dropped_,
            flags};
  }

private:
  std::array<AudioEvent, kSlots> ring_{};
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
  std::uint32_t high_water_ = 0;
  std::uint32_t dropped_ = 0;
};

} // namespace tessera::md
