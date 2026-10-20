#pragma once
#include "domain/wire.hpp"
#include "systems/system_registry.hpp"
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace tessera {
struct Budget {
  std::atomic<std::uint64_t> bytes{};
  std::atomic<unsigned> count{};
};
struct Snapshot {
  tessera_snapshot_info_v1 info{};
  std::vector<RegionData> regions;
  std::shared_ptr<Budget> budget;
  ~Snapshot() {
    if (budget) {
      budget->bytes.fetch_sub(info.retained_bytes);
      budget->count.fetch_sub(1);
    }
  }
};
struct Pending {
  std::uint64_t id, epoch, frame, regions, events;
};
struct Event {
  std::uint64_t sequence;
  Bytes record;
};
class Session {
public:
  tessera_session handle{};
  std::uint64_t epoch{1}, generation{1};
  std::string backend;
  Bytes content, config, memory, content_sha, config_sha, build, initial_sha,
      replay_raw;
  std::unique_ptr<IEmulatedSystem> machine;
  std::optional<Pending> pending;
  std::optional<wire::Trace> replay;
  std::uint64_t replay_next{1}, region_mask{}, event_mask{};
  std::shared_ptr<Budget> budget = std::make_shared<Budget>();
  std::shared_ptr<Snapshot> latest;
  bool reset_fault{};
  std::deque<Event> events;
  std::uint64_t sequence{}, loss_count{}, loss_first{}, loss_last{},
      loss_epoch{};
  std::uint32_t loss_cause{};
  bool range_known{true};
  Session(std::string id, wire::View rom, wire::View cfg, wire::View pm,
          wire::View build_id);
  [[nodiscard]] tessera_frame_id_v1 identity() const;
  [[nodiscard]] tessera_session_info_v1 info() const;
  [[nodiscard]] Bytes state() const;
  void validate_target(std::uint64_t requested_epoch,
                       std::uint64_t frame) const;
  [[nodiscard]] InputState
      validate_inputs(std::span<const tessera_input_assignment_v1>) const;
  void step(const InputState &, tessera_context_v1 &);
  void reset(tessera_context_v1 &);
  [[nodiscard]] Bytes checkpoint() const;
  void restore(wire::View checkpoint, wire::View trace,
               tessera_context_v1 &context);
  void cancel(tessera_context_v1 &, std::uint32_t reason);
  void capture() noexcept;
  void emit(std::uint32_t code) noexcept;
  void loss(std::uint32_t cause, std::uint64_t first = 0,
            std::uint64_t last = 0) noexcept;
  [[nodiscard]] Bytes losses() const;
  [[nodiscard]] Bytes event_batch(std::uint64_t cursor,
                                  std::uint64_t &next) const;
};
// Token type bits and monotonic generations are values, never addresses.
template <class T, std::uint64_t Kind> class Handles {
  struct Entry {
    std::shared_ptr<T> value;
    std::uint64_t references{1};
  };
  std::mutex mutex_;
  std::unordered_map<std::uint64_t, Entry> entries_;
  std::uint64_t next_{1};

public:
  std::uint64_t add(std::shared_ptr<T> v) {
    std::lock_guard lock(mutex_);
    for (auto &[token, entry] : entries_)
      if (entry.value == v) {
        require(entry.references < UINT64_MAX, TESSERA_BUDGET_EXCEEDED);
        ++entry.references;
        return token;
      }
    require(next_ < (UINT64_C(1) << 56), TESSERA_BUDGET_EXCEEDED);
    const auto token = (Kind << 56) | next_++;
    entries_.emplace(token, Entry{std::move(v)});
    return token;
  }
  std::shared_ptr<T> get(std::uint64_t token) {
    std::lock_guard lock(mutex_);
    const auto it = entries_.find(token);
    require(it != entries_.end(), TESSERA_INVALID_HANDLE);
    return it->second.value;
  }
  void retain(std::uint64_t token) {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(token);
    require(it != entries_.end(), TESSERA_INVALID_HANDLE);
    require(it->second.references < UINT64_MAX, TESSERA_BUDGET_EXCEEDED);
    ++it->second.references;
  }
  void release(std::uint64_t token) {
    std::shared_ptr<T> retired;
    {
      std::lock_guard lock(mutex_);
      auto it = entries_.find(token);
      require(it != entries_.end(), TESSERA_INVALID_HANDLE);
      if (--it->second.references == 0) {
        retired = std::move(it->second.value);
        entries_.erase(it);
      }
    }
  }
};
inline Handles<Session, 0xA1> sessions;
inline Handles<Snapshot, 0xA2> snapshots;
} // namespace tessera
