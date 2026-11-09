// Cost and memory of the native Mega Drive machine through the public API
// (spec 008, TP-208, RNF-1, RNF-2). The counterpart of
// `tessera_diffbench --libretro ... --bench-out`, which timed gpgx's retro_run
// for the baseline B and B0 (TP-014): here each tessera_session_step_v1 is
// timed, with every subscription (full, TESSERA_OBS_SUB_ALL) or none, and no
// pad input, as the gpgx run had.
//   tessera_md_bench --rom <file> [--frames N] [--warmup N]
//                    [--observation full|none] --bench-out <file.json>
// Memory: live heap bytes counted by this program's operator new/delete
// (_msize), so the session's resident state is what the session holds after
// creation and one frame without subscriptions, less its two ROM copies; the
// observation buffers are what subscribing to everything adds after a frame.
#include "domain/wire.hpp"
#include <tessera/tessera.h>

#include <nlohmann/json.hpp>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <malloc.h>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::atomic<std::int64_t> g_live{0};
std::atomic<std::int64_t> g_peak{0};

void note(std::int64_t delta) noexcept {
  const auto now = g_live.fetch_add(delta, std::memory_order_relaxed) + delta;
  auto peak = g_peak.load(std::memory_order_relaxed);
  while (now > peak &&
         !g_peak.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
  }
}
} // namespace

void *operator new(std::size_t size) {
  if (void *p = std::malloc(size == 0 ? 1 : size)) {
    note(static_cast<std::int64_t>(_msize(p)));
    return p;
  }
  throw std::bad_alloc{};
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept {
  if (p) {
    note(-static_cast<std::int64_t>(_msize(p)));
    std::free(p);
  }
}
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete(void *p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::size_t) noexcept { ::operator delete(p); }

namespace {
template <class T> T initial() {
  T value{};
  value.header = {sizeof(T), TESSERA_ABI_MAJOR, TESSERA_ABI_MINOR};
  return value;
}

[[noreturn]] void fail(const std::string &what, tessera_status status = 0) {
  std::cerr << "error: " << what;
  if (status != 0)
    std::cerr << " (status " << status << ")";
  std::cerr << "\n";
  std::exit(1);
}

struct Session {
  tessera_context_v1 context = initial<tessera_context_v1>();
  tessera_session handle{};

  explicit Session(const tessera::Bytes &rom) {
    std::array<tessera_backend_info_v1, 4> backends{
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>(),
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>()};
    std::uint32_t required{};
    if (const auto s = tessera_backends_enumerate_v1(
            backends.data(), static_cast<std::uint32_t>(backends.size()),
            &required, &context);
        s != TESSERA_OK)
      fail("backends", s);
    const tessera_backend_info_v1 *native = nullptr;
    for (std::uint32_t i = 0; i < required && i < backends.size(); ++i)
      if (std::string_view(backends[i].id) == "megadrive")
        native = &backends[i];
    if (!native)
      fail("this build has no megadrive backend");
    const tessera::Bytes memory(65536, 0);
    const auto configuration =
        tessera::wire::configuration("megadrive", native->build_id, memory);
    auto create = initial<tessera_session_create_info_v1>();
    const auto id = tessera::wire::text("megadrive");
    create.backend_id = {id.data(), id.size()};
    create.content = {rom.data(), rom.size()};
    create.config = {configuration.data(), configuration.size()};
    create.initial_persistent_memory = {memory.data(), memory.size()};
    create.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
    if (const auto s = tessera_session_create_v1(&create, &handle, &context);
        s != TESSERA_OK)
      fail("session_create", s);
  }
  ~Session() { tessera_session_close_v1(handle, &context); }
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  void subscribe(std::uint32_t mask) {
    if (const auto s =
            tessera_observation_subscriptions_set_v1(handle, mask, &context);
        s != TESSERA_OK)
      fail("subscriptions_set", s);
  }
  /// One frame, every control released (the gpgx run had no input).
  void step() {
    auto info = initial<tessera_session_info_v1>();
    if (const auto s = tessera_session_query_v1(handle, &info, &context);
        s != TESSERA_OK)
      fail("session_query", s);
    std::array<tessera_input_assignment_v1, 24> assignments{};
    for (std::uint16_t port = 0; port < 2; ++port)
      for (std::uint16_t control = 0; control < 12; ++control)
        assignments[port * 12 + control] = {port, control, 0};
    auto input = initial<tessera_input_frame_v1>();
    input.epoch = info.frame_id.epoch;
    input.frame = info.frame_id.frame + 1;
    input.assignments = assignments.data();
    input.count = static_cast<std::uint32_t>(assignments.size());
    if (const auto s = tessera_session_step_v1(handle, &input, &context);
        s != TESSERA_OK)
      fail("session_step", s);
  }
  std::uint64_t savestate_bytes() {
    std::uint64_t required{};
    tessera_checkpoint_export_v1(handle, {nullptr, 0}, &required, &context);
    return required;
  }
};

std::uint64_t process_peak() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  GetProcessMemoryInfo(GetCurrentProcess(),
                       reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                       sizeof(counters));
  return counters.PeakWorkingSetSize;
}
} // namespace

int main(int argc, char **argv) {
  std::string rom_path, out_path, mode = "full";
  unsigned frames = 600, warmup = 60;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool value = i + 1 < argc;
    if (arg == "--rom" && value)
      rom_path = argv[++i];
    else if (arg == "--bench-out" && value)
      out_path = argv[++i];
    else if (arg == "--observation" && value)
      mode = argv[++i];
    else if (arg == "--frames" && value)
      frames = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    else if (arg == "--warmup" && value)
      warmup = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    else {
      std::cerr << "usage: tessera_md_bench --rom <file> [--frames N] "
                   "[--warmup N] [--observation full|none] --bench-out "
                   "<file.json>\n";
      return 2;
    }
  }
  if (rom_path.empty() || out_path.empty() ||
      (mode != "full" && mode != "none")) {
    std::cerr << "--rom, --bench-out and --observation full|none\n";
    return 2;
  }
  std::ifstream file(rom_path, std::ios::binary);
  if (!file)
    fail("cannot read " + rom_path);
  const tessera::Bytes rom{std::istreambuf_iterator<char>(file), {}};

  const auto before = g_live.load();
  Session session(rom);
  session.subscribe(0);
  session.step();
  // The session holds the ROM twice: the session's content (identity, reset,
  // traces) and the cartridge the bus reads. RNF-2 counts the state without
  // the ROM, so both copies come off.
  const auto session_live = g_live.load() - before;
  const auto resident =
      session_live - 2 * static_cast<std::int64_t>(rom.size());
  session.subscribe(TESSERA_OBS_SUB_ALL);
  const auto unsubscribed = g_live.load();
  session.step();
  const auto observation_buffers = g_live.load() - unsubscribed;
  session.subscribe(mode == "full" ? TESSERA_OBS_SUB_ALL : 0u);

  auto obs_limits = initial<tessera_observation_limits_v1>();
  const auto limits_status = tessera_observation_limits_query_v1(
      session.handle, &obs_limits, &session.context);

  std::vector<double> micros;
  micros.reserve(frames);
  for (unsigned i = 0; i < frames; ++i) {
    const auto t0 = std::chrono::steady_clock::now();
    session.step();
    const auto t1 = std::chrono::steady_clock::now();
    micros.push_back(
        std::chrono::duration<double, std::micro>(t1 - t0).count());
  }
  std::vector<double> kept(micros.begin() +
                               std::min<std::size_t>(warmup, micros.size()),
                           micros.end());
  std::sort(kept.begin(), kept.end());
  const auto pct = [&](double q) {
    return kept.empty()
               ? 0.0
               : kept[std::min(kept.size() - 1,
                               static_cast<std::size_t>(q * kept.size()))];
  };
  nlohmann::json j;
  j["rom"] = rom_path;
  j["core"] = "tessera megadrive";
  j["frames"] = frames;
  j["warmup"] = warmup;
  j["observation"] = mode;
  j["measured_frames"] = kept.size();
  j["p50_us"] = pct(0.50);
  j["p95_us"] = pct(0.95);
  j["p99_us"] = pct(0.99);
  j["savestate_bytes"] = session.savestate_bytes();
  j["rom_bytes"] = rom.size();
  j["session_live_bytes"] = session_live;
  j["resident_state_bytes"] = resident;
  j["observation_buffer_bytes"] = observation_buffers;
  j["heap_peak_bytes"] = g_peak.load();
  j["process_peak_bytes"] = process_peak();
  j["observation_limits_status"] = limits_status;
  if (limits_status == TESSERA_OK) {
    j["max_rom_bytes"] = obs_limits.max_rom_bytes;
    j["max_state_bytes"] = obs_limits.max_state_bytes;
    j["max_journal_events"] = obs_limits.max_journal_events;
    j["max_audio_writes"] = obs_limits.max_audio_writes;
    j["max_parsed_sprites"] = obs_limits.max_parsed_sprites;
  }
  std::ofstream out(out_path);
  out << j.dump(2) << "\n";
  std::cout << "p50_us=" << j["p50_us"] << " p99_us=" << j["p99_us"]
            << " savestate=" << j["savestate_bytes"] << " resident=" << resident
            << "\n";
  return 0;
}
