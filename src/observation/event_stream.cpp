#include "api/call.hpp"
#include "session/session.hpp"
namespace tessera {
using namespace wire;
void Session::loss(std::uint32_t cause, std::uint64_t first,
                   std::uint64_t last) noexcept {
  ++loss_count;
  loss_epoch = epoch;
  loss_cause |= cause;
  if (first == 0) {
    range_known = false;
    loss_first = loss_last = 0;
  } else if (range_known) {
    if (!loss_first)
      loss_first = first;
    loss_last = last;
  }
}
void Session::emit(std::uint32_t code) noexcept {
  if (!(event_mask & (UINT64_C(1) << (code - 1))))
    return;
  try {
    require(sequence < UINT64_MAX, TESSERA_BUDGET_EXCEEDED);
    const auto next = sequence + 1;
    Bytes b;
    scalar(b, 1, 3, next);
    scalar(b, 2, 3, handle);
    scalar(b, 3, 3, epoch);
    scalar(b, 4, 3, machine->frame());
    scalar(b, 5, 3, machine->tick());
    scalar(b, 6, 1, code);
    field(b, 7, 5, {});
    scalar(b, 8, 1, 1);
    events.push_back({next, std::move(b)});
    sequence = next;
    if (events.size() > 4096) {
      loss(1, events.front().sequence, events.front().sequence);
      events.pop_front();
    }
  } catch (...) {
    loss(1);
  }
}
Bytes Session::losses() const {
  Bytes b;
  scalar(b, 1, 3, loss_count);
  scalar(b, 2, 3, range_known ? loss_first : 0);
  scalar(b, 3, 3, range_known ? loss_last : 0);
  scalar(b, 4, 1, loss_cause | (range_known ? UINT32_C(0x80000000) : 0));
  scalar(b, 5, 3, loss_epoch);
  return b;
}
Bytes Session::event_batch(std::uint64_t cursor, std::uint64_t &next) const {
  require(cursor <= sequence);
  std::vector<Bytes> available;
  for (const auto &event : events)
    if (event.sequence > cursor)
      available.push_back(event.record);
  next = sequence;
  Bytes b;
  field(b, 1, 8, array(available));
  scalar(b, 2, 3, next);
  field(b, 3, 11, losses());
  return b;
}
} // namespace tessera
