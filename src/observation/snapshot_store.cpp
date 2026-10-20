#include "api/call.hpp"
#include "session/session.hpp"
namespace tessera {
using namespace wire;
void Session::capture() noexcept {
  latest.reset();
  try {
    if (!(machine->capabilities() & TESSERA_CAP_MEMORY))
      return;
    auto regions = machine->regions(region_mask);
    std::uint64_t bytes{};
    for (const auto &r : regions)
      bytes += r.data.size();
    if (budget->count.load() >= 8 || bytes > 8 * 1024 * 1024 ||
        budget->bytes.load() > 64 * 1024 * 1024 - bytes) {
      loss(2);
      emit(5);
      return;
    }
    auto s = std::make_shared<Snapshot>();
    s->info = api::initialized<tessera_snapshot_info_v1>();
    s->info.frame_id = identity();
    s->info.generation = generation;
    s->info.region_mask = s->info.complete_mask = region_mask;
    for (const auto &r : regions)
      if (r.kind == 5 && (r.flags & 0x80000000U) != 0)
        s->info.complete_mask &= ~TESSERA_REGION_TEMPORAL;
    s->info.retained_bytes = bytes;
    s->info.logical_tick = machine->tick();
    s->info.region_count = static_cast<std::uint32_t>(regions.size());
    s->regions = std::move(regions);
    budget->count.fetch_add(1);
    budget->bytes.fetch_add(bytes);
    s->budget = budget;
    latest = std::move(s);
    emit(4);
  } catch (...) {
    loss(2);
  }
}
} // namespace tessera
