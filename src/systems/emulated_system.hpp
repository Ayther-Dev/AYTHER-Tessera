#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tessera {

using Bytes = std::vector<std::uint8_t>;
/// Controls of a pad in the native Mega Drive layout, in this order: up, down,
/// left, right, B, C, A, start, X, Y, Z, mode (TP-108.3).
inline constexpr std::uint32_t kLegacyControls = 8;
inline constexpr std::uint32_t kPadControls = 12;
inline constexpr std::uint32_t kPadPorts = 2;
/// Shape of the input a system takes: `ports` pads of `controls` controls,
/// stored port-major in an InputState. The legacy layout (one port, eight
/// controls) is the one of the synthetic and Clown backends and of their
/// traces; the native Mega Drive uses two ports of twelve.
struct InputLayout {
  std::uint32_t ports;
  std::uint32_t controls;
  constexpr std::uint32_t count() const noexcept { return ports * controls; }
  constexpr bool operator==(const InputLayout &) const noexcept = default;
};
inline constexpr InputLayout kLegacyInputLayout{1, kLegacyControls};
inline constexpr InputLayout kPadsInputLayout{kPadPorts, kPadControls};
inline constexpr std::size_t kInputStateSize = kPadPorts * kPadControls;
using InputState = std::array<std::int32_t, kInputStateSize>;
/// Layout told apart by the number of assignments a record carries.
inline constexpr bool input_layout_from_count(std::size_t count,
                                              InputLayout &out) noexcept {
  if (count == kLegacyInputLayout.count()) {
    out = kLegacyInputLayout;
    return true;
  }
  if (count == kPadsInputLayout.count()) {
    out = kPadsInputLayout;
    return true;
  }
  return false;
}

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

/// Observation surface of a system (spec 008, RF-3, TP-201.3). Statuses are
/// tessera_status values (0 = ok). Observing never changes the emulated state;
/// `observation_work()` counts what the observation did (bytes it produced), so
/// a system with nothing subscribed reports 0.
class IObservable {
public:
  virtual ~IObservable() = default;
  /// False if the mask has bits the contract lacks; nothing changes then.
  virtual bool set_subscriptions(std::uint32_t mask) = 0;
  struct Subscriptions {
    std::uint32_t supported, active, requested;
    std::uint64_t activation_frame;
  };
  [[nodiscard]] virtual Subscriptions subscriptions() const = 0;
  /// NOT_SUBSCRIBED while the region's subscription is idle, UNSUPPORTED while
  /// the system does not serve it yet.
  [[nodiscard]] virtual std::int32_t
  read_region(std::uint32_t id, std::uint32_t offset,
              std::span<std::uint8_t> out) = 0;
  /// Current byte size of a region whose size varies (ATTRIBUTION, LINE_*),
  /// as the reference answers in its query, possibly 0; UINT32_MAX for the
  /// table size.
  [[nodiscard]] virtual std::uint32_t
  region_byte_size(std::uint32_t /*id*/) const {
    return UINT32_MAX;
  }
  /// `event_size` bytes per event; `count` events written (0 if none).
  [[nodiscard]] virtual std::int32_t
  poll_audio_events(std::span<std::uint8_t> out, std::uint32_t capacity,
                    std::uint32_t event_size, std::uint32_t &count) = 0;
  /// 32 bytes of ayther_audio_transport_stats_v1; ok even when idle.
  [[nodiscard]] virtual std::int32_t
  audio_transport_stats(std::span<std::uint8_t> out) const = 0;
  [[nodiscard]] virtual std::uint64_t observation_work() const noexcept = 0;
};

class IEmulatedSystem {
public:
  virtual ~IEmulatedSystem() = default;
  [[nodiscard]] virtual std::string_view backend_id() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t capabilities() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t frame() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t tick() const noexcept = 0;
  [[nodiscard]] virtual const InputState &latched_inputs() const noexcept = 0;
  /// Ports and controls the system takes (legacy: one port of eight).
  [[nodiscard]] virtual InputLayout input_layout() const noexcept {
    return kLegacyInputLayout;
  }
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
  /// Null when the system serves no observation (the default).
  [[nodiscard]] virtual IObservable *observation() noexcept { return nullptr; }
};

} // namespace tessera
