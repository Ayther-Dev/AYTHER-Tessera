#include "libretro_host.hpp"

#include <windows.h>

#include "tessera/tessera_observation.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace diffbench {
namespace {

struct GameInfo {
  const char *path;
  const void *data;
  size_t size;
  const char *meta;
};

using EnvCb = bool(__cdecl *)(unsigned, void *);
using VideoCb = void(__cdecl *)(const void *, unsigned, unsigned, size_t);
using AudioCb = void(__cdecl *)(int16_t, int16_t);
using AudioBatchCb = size_t(__cdecl *)(const int16_t *, size_t);
using PollCb = void(__cdecl *)();
using InputCb = int16_t(__cdecl *)(unsigned, unsigned, unsigned, unsigned);

// Espejo de ayther_interface_v1 hasta set_subscriptions (mismo orden y
// alineación naturales que el header de referencia).
struct AytherInterface {
  uint32_t abi_version, struct_size;
  uint64_t capabilities;
  uint32_t host_endianness, pointer_size, region_info_size, frame_snapshot_size,
      sprite_size, audio_write_size;
  const char *build_id;
  uint32_t build_id_size, reserved0;
  int32_t(__cdecl *query_region)(uint32_t, tessera_obs_region_info_v1 *,
                                 uint32_t);
  int32_t(__cdecl *read_region)(uint32_t, uint32_t, void *, uint32_t, uint64_t,
                                uint64_t *);
  int32_t(__cdecl *write_control)(uint32_t, uint32_t, const void *, uint32_t,
                                  uint64_t, uint64_t *);
  void *capture_snapshot;
  int32_t(__cdecl *recompose_frame)(uint16_t *, uint32_t, uint32_t, uint32_t *,
                                    uint32_t *);
  uint32_t audio_event_size, audio_transport_stats_size;
  void *poll_audio_events;
  void *get_audio_transport_stats;
  uint32_t subscription_state_size, reserved1;
  void *get_subscriptions;
  int32_t(__cdecl *set_subscriptions)(uint32_t);
  // ABI 1.0 tail, 1.1 and 1.2 (read only when struct_size reaches them)
  uint32_t frame_delta_size;
  void *poll_frame_delta;
  uint32_t recompose_stats_size, reserved2;
  void *get_recompose_stats;
  int32_t(__cdecl *recompose_multilayer)(uint16_t *, uint16_t *, uint16_t *,
                                         uint16_t *, uint16_t *, uint32_t,
                                         uint32_t, uint32_t *, uint32_t *);
  // ABI 1.4
  int32_t(__cdecl *frame_delta_since)(uint64_t, void *, uint32_t);
};

/// The dirty bytes of a frame delta as runs "first-last:value" (one index
/// alone as "index:value"), comma separated; "none" when nothing is dirty.
std::string dirty_runs(const uint8_t *dirty, size_t count) {
  std::string out;
  char buffer[32];
  for (size_t i = 0; i < count;) {
    if (dirty[i] == 0) {
      ++i;
      continue;
    }
    size_t j = i;
    while (j + 1 < count && dirty[j + 1] == dirty[i])
      ++j;
    if (j == i)
      std::snprintf(buffer, sizeof(buffer), "%zu:%02x", i, dirty[i]);
    else
      std::snprintf(buffer, sizeof(buffer), "%zu-%zu:%02x", i, j, dirty[i]);
    out += out.empty() ? "" : ",";
    out += buffer;
    i = j + 1;
  }
  return out.empty() ? "none" : out;
}

RunResult *g_result = nullptr;
const std::vector<TraceFrame> *g_trace = nullptr;
size_t g_frame = 0;

uint64_t fnv1a(const uint8_t *p, size_t n, uint64_t h) {
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

bool __cdecl environment(unsigned cmd, void *data) {
  switch (cmd) {
  case 10: // SET_PIXEL_FORMAT
    if (std::getenv("TESSERA_DEBUG_VIDEO") && data)
      std::fprintf(stderr, "set_pixel_format %d%c",
                   *static_cast<int32_t *>(data), 10);
    if (data && std::getenv("TESSERA_ACCEPT_565"))
      return *static_cast<int32_t *>(data) == 2;
    return data && *static_cast<int32_t *>(data) == 1; // XRGB8888
  case 9:                                              // GET_SYSTEM_DIRECTORY
  case 31:                                             // GET_SAVE_DIRECTORY
    *static_cast<const char **>(data) = ".";
    return true;
  case 52: // GET_CORE_OPTIONS_VERSION
    *static_cast<unsigned *>(data) = 0;
    return true;
  default:
    return false; // GET_VARIABLE incluido: valores por defecto del core
  }
}

// TP-206.1: FNV-1a 64 over what the core emitted in the last frame, computed
// by the host: the 16-bit pixels row by row (w pixels, skipping the pitch)
// and the audio samples as delivered.
// Only with a --stats-script (the h/H operations need them): the bench
// timings of retro_run (TP-014, TP-208) leave them out, as the baseline did.
bool g_host_hash = false;
uint64_t g_video_fnv = 0, g_audio_fnv = 0, g_video_prev_fnv = 0;
unsigned g_prev_w = 0, g_prev_h = 0; // geometry of the frame before
uint64_t fnv_bytes(uint64_t h, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; ++i)
    h = (h ^ p[i]) * 1099511628211ull;
  return h;
}
void __cdecl video(const void *data, unsigned w, unsigned h, size_t pitch) {
  if (data && g_host_hash) {
    g_video_fnv = g_video_prev_fnv = fnv_seed;
    for (unsigned y = 0; y < h; ++y)
      g_video_fnv = fnv_bytes(g_video_fnv,
                              static_cast<const uint8_t *>(data) + y * pitch,
                              size_t{w} * 2);
    for (unsigned y = 0; y < g_prev_h; ++y)
      g_video_prev_fnv = fnv_bytes(
          g_video_prev_fnv, static_cast<const uint8_t *>(data) + y * pitch,
          size_t{g_prev_w} * 2);
    g_prev_w = w;
    g_prev_h = h;
  }
  if (!g_result)
    return;
  uint64_t hash = fnv_seed;
  if (data) {
    const auto *rows = static_cast<const uint8_t *>(data);
    // XRGB8888 little-endian en memoria: B, G, R, X.
    for (unsigned y = 0; y < h; ++y)
      for (unsigned x = 0; x < w; ++x) {
        const uint8_t *px = rows + y * pitch + size_t{x} * 4;
        hash = fnv_rgb(hash, px[2], px[1], px[0]);
      }
  }
  if (data) {
    const auto *rows = static_cast<const uint8_t *>(data);
    auto &out = g_result->last_frame;
    out.clear();
    for (const unsigned v : {w, h, static_cast<unsigned>(pitch)})
      for (unsigned b = 0; b < 4; ++b)
        out.push_back(static_cast<uint8_t>(v >> (8 * b)));
    out.insert(out.end(), rows, rows + size_t{h} * pitch);
  }
  g_result->width = w;
  g_result->height = h;
  g_result->frame_hashes.push_back(hash);
}
void note_peak(int v) {
  if (g_result && std::abs(v) > g_result->peak)
    g_result->peak = std::abs(v);
}
void __cdecl audio(int16_t l, int16_t r) {
  note_peak(l);
  note_peak(r);
}
size_t __cdecl audio_batch(const int16_t *data, size_t frames) {
  if (g_host_hash)
    g_audio_fnv = fnv_bytes(g_audio_fnv,
                            reinterpret_cast<const uint8_t *>(data), frames * 4);
  for (size_t i = 0; i < frames * 2; ++i)
    note_peak(data[i]);
  return frames;
}
// Cambios de mascara por fotograma (--pad-script): desde el fotograma `frame`
// los dos puertos valen mask[0] y mask[1]. El sondeo de gpgx ocurre justo antes
// de la V-int, al comienzo de su fotograma.
struct PadStep {
  unsigned frame;
  uint32_t mask[2];
};
std::vector<PadStep> &pad_script() {
  static std::vector<PadStep> script;
  return script;
}
uint32_t g_pad_masks[2] = {0, 0};
void __cdecl poll() {
  ++g_frame;
  for (const auto &step : pad_script())
    if (step.frame <= g_frame) {
      g_pad_masks[0] = step.mask[0];
      g_pad_masks[1] = step.mask[1];
    }
}
// Controles de la API (up down left right A B C start) -> ids libretro de gpgx
// (A=JOYPAD_Y, B=JOYPAD_B, C=JOYPAD_A, start=START).
// Mega Drive bit -> id libretro: arriba, abajo, izquierda, derecha, B, C, A,
// start, X, Y, Z, mode.
constexpr unsigned kPadIds[12] = {4, 5, 6, 7, 0, 8, 1, 3, 10, 9, 11, 2};
int16_t __cdecl input(unsigned port, unsigned device, unsigned, unsigned id) {
  if (port < 2 && device == 1 && g_pad_masks[port] != 0 && g_frame != 0) {
    for (unsigned bit = 0; bit < 12; ++bit)
      if (kPadIds[bit] == id)
        return static_cast<int16_t>((g_pad_masks[port] >> bit) & 1U);
    return 0;
  }
  if (port != 0 || device != 1 || !g_trace || g_frame == 0 ||
      g_frame > g_trace->size())
    return 0;
  static constexpr unsigned ids[8] = {4, 5, 6, 7, 1, 0, 8, 3};
  const auto &row = (*g_trace)[g_frame - 1];
  for (size_t i = 0; i < 8; ++i)
    if (ids[i] == id)
      return static_cast<int16_t>(row[i] != 0);
  return 0;
}

template <class F> bool sym(HMODULE m, const char *name, F &out) {
  out = reinterpret_cast<F>(GetProcAddress(m, name));
  return out != nullptr;
}

} // namespace

unsigned g_pad_device = 0;
void set_pad_device(unsigned device) { g_pad_device = device; }
static std::string g_audio_events_out;
void set_audio_events_out(const std::string &path) { g_audio_events_out = path; }
static std::string g_recompose_out;
static uint32_t g_recompose_flags = 0;
static uint32_t g_recompose_capacity = 320u * 240u;
static std::string g_multilayer_out;
void set_multilayer_out(const std::string &path) { g_multilayer_out = path; }
static std::map<unsigned, std::vector<std::string>> g_stats_script;
static std::string g_stats_out;
void set_stats_script(const std::string &script, const std::string &out) {
  // <frame>:<op>.<op>...;<frame>:... with s = recompose_frame, m =
  // recompose_multilayer, g = get_recompose_stats, p<region>=<hex byte> =
  // write_control, run after that frame (0 = the first).
  std::stringstream entries(script);
  for (std::string entry; std::getline(entries, entry, ';');) {
    const auto colon = entry.find(':');
    if (colon == std::string::npos)
      continue;
    auto &ops = g_stats_script[static_cast<unsigned>(
        std::stoul(entry.substr(0, colon)))];
    std::stringstream list(entry.substr(colon + 1));
    for (std::string op; std::getline(list, op, '.');)
      ops.push_back(op);
  }
  g_stats_out = out;
}
void set_recompose_out(const std::string &path, uint32_t flags,
                       uint32_t capacity) {
  g_recompose_out = path;
  g_recompose_flags = flags;
  g_recompose_capacity = capacity;
}
static unsigned g_reset_at = 0;
void set_reset_at(unsigned frame) { g_reset_at = frame; }
static bool g_bench = false;
static int g_bench_observation = -1;
static int64_t g_sub_mask = -1;
void set_subscription_mask(int64_t mask) { g_sub_mask = mask; }
void set_bench(bool on, int observation) {
  g_bench = on;
  g_bench_observation = observation;
}

void set_pad_script(unsigned frame, uint32_t port0, uint32_t port1) {
  pad_script().push_back({frame, {port0, port1}});
}

void set_pad_masks(uint32_t port0, uint32_t port1) {
  g_pad_masks[0] = port0;
  g_pad_masks[1] = port1;
}

RunResult run_libretro(const std::string &core_path,
                       const std::string &rom_path, unsigned frames,
                       bool dump_regions,
                       const std::vector<TraceFrame> *trace, std::vector<Poke> *pokes) {
  RunResult result;
  std::ifstream in(rom_path, std::ios::binary);
  if (!in) {
    result.error = "no se puede abrir la ROM " + rom_path;
    return result;
  }
  std::vector<char> rom((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
  HMODULE core = LoadLibraryA(core_path.c_str());
  if (!core) {
    result.error = "no se puede cargar el core " + core_path;
    return result;
  }
  void(__cdecl * set_env)(EnvCb) = nullptr;
  void(__cdecl * set_video)(VideoCb) = nullptr;
  void(__cdecl * set_audio)(AudioCb) = nullptr;
  void(__cdecl * set_batch)(AudioBatchCb) = nullptr;
  void(__cdecl * set_poll)(PollCb) = nullptr;
  void(__cdecl * set_input)(InputCb) = nullptr;
  void(__cdecl * init)() = nullptr;
  bool(__cdecl * load)(const GameInfo *) = nullptr;
  void(__cdecl * run)() = nullptr;
  void(__cdecl * reset_core)() = nullptr;
  void(__cdecl * unload)() = nullptr;
  void(__cdecl * deinit)() = nullptr;
  if (!(sym(core, "retro_set_environment", set_env) &&
        sym(core, "retro_set_video_refresh", set_video) &&
        sym(core, "retro_set_audio_sample", set_audio) &&
        sym(core, "retro_set_audio_sample_batch", set_batch) &&
        sym(core, "retro_set_input_poll", set_poll) &&
        sym(core, "retro_set_input_state", set_input) &&
        sym(core, "retro_init", init) && sym(core, "retro_load_game", load) &&
        sym(core, "retro_run", run) && sym(core, "retro_unload_game", unload) &&
        sym(core, "retro_deinit", deinit))) {
    result.error = "el core no exporta la API libretro";
    FreeLibrary(core);
    return result;
  }
  g_result = &result;
  g_trace = trace;
  g_frame = 0;
  set_env(environment);
  init();
  set_video(video);
  set_audio(audio);
  set_batch(audio_batch);
  set_poll(poll);
  set_input(input);
  const GameInfo info{rom_path.c_str(), rom.data(), rom.size(), nullptr};
  if (!load(&info)) {
    result.error = "retro_load_game falló";
  } else {
    if (g_pad_device != 0) {
      void(__cdecl * set_port_device)(unsigned, unsigned) = nullptr;
      if (sym(core, "retro_set_controller_port_device", set_port_device)) {
        set_port_device(0, g_pad_device);
        set_port_device(1, g_pad_device);
      }
    }
    using GetIface = const AytherInterface *(__cdecl *)(uint32_t);
    GetIface get_iface = nullptr;
    const AytherInterface *iface = nullptr;
    const bool want_iface =
        dump_regions || pokes != nullptr || g_bench_observation >= 0 ||
        g_sub_mask >= 0 ||
        !g_audio_events_out.empty() || !g_recompose_out.empty() ||
        !g_multilayer_out.empty() || !g_stats_script.empty();
    if (want_iface && sym(core, "ayther_get_interface", get_iface))
      iface = get_iface(TESSERA_OBS_ABI_VERSION_1_10);
    if (want_iface && !iface)
      result.error = "el core no ofrece la interfaz AYTHER 1.10";
    if (iface) {
      result.ayther_abi = iface->abi_version;
      result.ayther_capabilities = iface->capabilities;
      if (iface->build_id != nullptr)
        result.ayther_build_id.assign(iface->build_id, iface->build_id_size);
      using GetSubs = int32_t(__cdecl *)(tessera_obs_subscription_state_v1 *,
                                         uint32_t);
      const auto get_subs = reinterpret_cast<GetSubs>(iface->get_subscriptions);
      const auto ask = [&](SubscriptionState &out) {
        if (!get_subs)
          return;
        tessera_obs_subscription_state_v1 s{};
        out.status = get_subs(&s, sizeof(s));
        out.struct_size = s.struct_size;
        out.version = s.state_version;
        out.supported = s.supported_mask;
        out.active = s.active_mask;
        out.requested = s.requested_mask;
        out.activation_frame = s.activation_frame;
      };
      if (g_sub_mask >= 0) {
        result.sub_set_status =
            iface->set_subscriptions(static_cast<uint32_t>(g_sub_mask));
        ask(result.sub_after_set);
      } else {
        iface->set_subscriptions(g_bench_observation == 0 ? 0u
                                                          : TESSERA_OBS_SUB_ALL);
      }
    }
    if (g_reset_at != 0 && !sym(core, "retro_reset", reset_core))
      result.error = "el core no exporta retro_reset";
    g_host_hash = !g_stats_script.empty();
    if (!g_stats_script.empty() && !reset_core)
      sym(core, "retro_reset", reset_core);
    std::vector<Poke> no_pokes;
    for (unsigned i = 0; i < frames && result.error.empty(); ++i) {
      if (g_reset_at != 0 && i == g_reset_at - 1 && reset_core)
        reset_core();
      for (Poke &poke : pokes ? *pokes : no_pokes) {
        if (i != poke.frame || !iface)
          continue;
        uint64_t generation = 0;
        poke.status = iface->write_control(
            poke.region, poke.offset, poke.data.data(),
            static_cast<uint32_t>(poke.data.size()), UINT64_MAX, &generation);
        poke.generation = generation;
      }
      const auto drain_audio_events = [&] {
        if (g_audio_events_out.empty() || !iface || !iface->poll_audio_events)
          return;
        using Poll = int32_t(__cdecl *)(void *, uint32_t, uint32_t *);
        const auto poll_events = reinterpret_cast<Poll>(iface->poll_audio_events);
        std::vector<uint8_t> buffer(80u * 1024u);
        std::ofstream out(g_audio_events_out, std::ios::binary | std::ios::app);
        for (;;) {
          uint32_t count = 0;
          if (poll_events(buffer.data(), 1024, &count) != 0 || count == 0)
            break;
          out.write(reinterpret_cast<const char *>(buffer.data()),
                    static_cast<std::streamsize>(count) * 80);
        }
      };
      g_audio_fnv = fnv_seed;
      if (g_bench) {
        const auto t0 = std::chrono::steady_clock::now();
        run();
        const auto t1 = std::chrono::steady_clock::now();
        result.run_micros.push_back(
            std::chrono::duration<double, std::micro>(t1 - t0).count());
      } else {
        run();
      }
      drain_audio_events();
      // TP-205.3: the stats script after this frame.
      const auto ops = g_stats_script.find(i);
      if (iface && ops != g_stats_script.end()) {
        std::ofstream so(g_stats_out, std::ios::app);
        std::vector<uint16_t> layers(5u * 320u * 240u);
        for (const auto &op : ops->second) {
          uint32_t w = 0, h = 0;
          int32_t status = -8;
          // s/m take optional "<flags hex>@<capacity>"; g takes "<out_size>";
          // u<hex> sets subscriptions; r resets the core.
          const auto arg = [&](size_t from, uint32_t def, int base) {
            return op.size() > from ? static_cast<uint32_t>(std::stoul(
                                          op.substr(from), nullptr, base))
                                    : def;
          };
          const auto at = op.find('@');
          const uint32_t cap =
              at == std::string::npos
                  ? 320u * 240u
                  : static_cast<uint32_t>(std::stoul(op.substr(at + 1)));
          const uint32_t flags =
              op.size() > 1 && op[1] != '@'
                  ? static_cast<uint32_t>(std::stoul(
                        op.substr(1, at == std::string::npos ? std::string::npos
                                                             : at - 1),
                        nullptr, 16))
                  : 0u;
          if (op[0] == 'u') {
            status = iface->set_subscriptions(arg(1, 0, 16));
          } else if (op == "x") {
            // serialize and restore the same state
            size_t(__cdecl * ssize)() = nullptr;
            bool(__cdecl * ser)(void *, size_t) = nullptr;
            bool(__cdecl * unser)(const void *, size_t) = nullptr;
            if (sym(core, "retro_serialize_size", ssize) &&
                sym(core, "retro_serialize", ser) &&
                sym(core, "retro_unserialize", unser)) {
              std::vector<uint8_t> state(ssize());
              status = ser(state.data(), state.size()) &&
                               unser(state.data(), state.size())
                           ? 0
                           : -1;
            }
          } else if (op == "S" || op == "L") {
            // S saves the state, L restores the one saved last
            static std::vector<uint8_t> saved;
            size_t(__cdecl * ssize)() = nullptr;
            bool(__cdecl * ser)(void *, size_t) = nullptr;
            bool(__cdecl * unser)(const void *, size_t) = nullptr;
            if (sym(core, "retro_serialize_size", ssize) &&
                sym(core, "retro_serialize", ser) &&
                sym(core, "retro_unserialize", unser)) {
              if (op == "S") {
                saved.assign(ssize(), 0);
                status = ser(saved.data(), saved.size()) ? 0 : -1;
              } else {
                status =
                    !saved.empty() && unser(saved.data(), saved.size()) ? 0 : -1;
              }
            }
          } else if (op == "r" && reset_core) {
            reset_core();
            status = 0;
          } else if (op[0] == 's' && iface->recompose_frame) {
            status = iface->recompose_frame(layers.data(), cap, flags, &w, &h);
          } else if (op[0] == 'm' &&
                     iface->struct_size >=
                         offsetof(AytherInterface, recompose_multilayer) +
                             sizeof(iface->recompose_multilayer) &&
                     iface->recompose_multilayer) {
            uint16_t *l = layers.data();
            const uint32_t n = 320u * 240u;
            status = iface->recompose_multilayer(l, l + n, l + 2 * n, l + 3 * n,
                                                 l + 4 * n, cap, flags, &w, &h);
          } else if (op[0] == 'g' &&
                     iface->struct_size >=
                         offsetof(AytherInterface, get_recompose_stats) +
                             sizeof(iface->get_recompose_stats) &&
                     iface->get_recompose_stats) {
            tessera_obs_recompose_stats_v1 st{};
            status = reinterpret_cast<int32_t(__cdecl *)(void *, uint32_t)>(
                iface->get_recompose_stats)(&st, arg(1, sizeof(st), 10));
            so << "frame=" << i << " stats status=" << status
               << " size=" << st.struct_size << " single=" << st.single_calls
               << "/" << st.single_hits << " multi=" << st.multilayer_calls
               << "/" << st.multilayer_hits << " fingerprint=" << std::hex
               << st.controls_fingerprint << std::dec << "\n";
            continue;
          } else if (op[0] == 'h' || op[0] == 'H') {
            // TP-206.1: the FRAME_HASH region against the host's own FNV-1a
            // of the emitted video and audio and of the VRAM, CRAM and VSRAM
            // regions (read raw, as the core lays them out).
            std::vector<uint8_t> fh(56, 0x5A);
            uint64_t generation = 0;
            status = iface->read_region(23, 0, fh.data(), 56, UINT64_MAX,
                                        &generation);
            const auto u64 = [&](size_t at) {
              uint64_t v = 0;
              std::memcpy(&v, fh.data() + at, 8);
              return v;
            };
            const auto memory = [&](uint32_t id, uint32_t size) {
              std::vector<uint8_t> m(size);
              uint64_t g = 0;
              return iface->read_region(id, 0, m.data(), size, UINT64_MAX,
                                        &g) == 0
                         ? fnv_bytes(fnv_seed, m.data(), m.size())
                         : 0;
            };
            uint32_t version = 0, size = 0;
            std::memcpy(&version, fh.data(), 4);
            std::memcpy(&size, fh.data() + 4, 4);
            so << "frame=" << i << " hash status=" << status
               << " version=" << version << " size=" << size
               << " frame_index=" << u64(8) << std::hex
               << " video=" << u64(16) << " audio=" << u64(24)
               << " vram=" << u64(32) << " cram=" << u64(40)
               << " vsram=" << u64(48);
            if (op[0] == 'H') // diagnostics: what the host computes itself
              so << " | host video=" << g_video_fnv
               << " audio=" << g_audio_fnv << " vram=" << memory(1, 65536)
               << " cram=" << memory(2, 128) << " vsram=" << memory(4, 128)
               << " video_prev_geometry=" << g_video_prev_fnv;
            so << std::dec << "\n";
            continue;
          } else if (op[0] == 'd' || op[0] == 'D') {
            // TP-205.4: d[out_size] polls the delta of the last frame;
            // D<generation>[,out_size] asks for the delta since a generation
            // ("D-<n>" counts back from the generation of the last poll).
            static uint64_t last_generation = 0;
            tessera_obs_frame_delta_v1 delta{};
            std::memset(&delta, 0x5A, sizeof(delta));
            const auto comma = op.find(',');
            const uint32_t size =
                comma != std::string::npos
                    ? static_cast<uint32_t>(std::stoul(op.substr(comma + 1)))
                : op[0] == 'd' && op.size() > 1
                    ? static_cast<uint32_t>(std::stoul(op.substr(1)))
                    : static_cast<uint32_t>(sizeof(delta));
            status = -8;
            if (op[0] == 'd' && iface->poll_frame_delta) {
              status = reinterpret_cast<int32_t(__cdecl *)(void *, uint32_t)>(
                  iface->poll_frame_delta)(&delta, size);
            } else if (op[0] == 'D' &&
                       iface->struct_size >=
                           offsetof(AytherInterface, frame_delta_since) +
                               sizeof(iface->frame_delta_since) &&
                       iface->frame_delta_since) {
              const std::string g = op.substr(
                  1, comma == std::string::npos ? std::string::npos : comma - 1);
              const uint64_t from =
                  g[0] == '-' ? last_generation - std::stoull(g.substr(1))
                              : std::stoull(g);
              status = iface->frame_delta_since(from, &delta, size);
            }
            if (op[0] == 'd' && status == 0)
              last_generation = delta.frame_generation;
            so << "frame=" << i << " " << op << " status=" << status
               << " size=" << delta.struct_size
               << " version=" << delta.delta_version
               << " generation=" << delta.frame_generation
               << " raster=" << delta.raster_event_count
               << " sprites=" << delta.parsed_sprite_count
               << " audio=" << delta.audio_write_count
               << " dropped=" << delta.raster_events_dropped << " dirty="
               << dirty_runs(delta.dirty_patterns,
                             sizeof(delta.dirty_patterns))
               << "\n";
            continue;
          } else if (op.size() > 2 && op[0] == 'p') {
            const auto eq = op.find('=');
            const uint8_t byte =
                static_cast<uint8_t>(std::stoul(op.substr(eq + 1), nullptr, 16));
            uint64_t generation = 0;
            status = iface->write_control(
                static_cast<uint32_t>(std::stoul(op.substr(1, eq - 1))), 0,
                &byte, 1, UINT64_MAX, &generation);
          }
          so << "frame=" << i << " " << op << " status=" << status << "\n";
        }
      }
    }
    if (!g_audio_events_out.empty() && iface &&
        iface->get_audio_transport_stats) {
      using Stats = int32_t(__cdecl *)(void *, uint32_t);
      tessera_obs_audio_transport_stats_v1 stats{};
      const int32_t status = reinterpret_cast<Stats>(
          iface->get_audio_transport_stats)(&stats, sizeof(stats));
      std::ofstream sf(g_audio_events_out + ".stats",
                       std::ios::binary | std::ios::trunc);
      sf.write(reinterpret_cast<const char *>(&status), sizeof(status));
      sf.write(reinterpret_cast<const char *>(&stats), sizeof(stats));
    }
    if (g_bench) {
      size_t(__cdecl * serialize_size)() = nullptr;
      if (sym(core, "retro_serialize_size", serialize_size))
        result.serialize_size = serialize_size();
    }
    if (iface && g_sub_mask >= 0 && iface->get_subscriptions) {
      tessera_obs_subscription_state_v1 s{};
      result.sub_end.status =
          reinterpret_cast<int32_t(__cdecl *)(tessera_obs_subscription_state_v1 *,
                                              uint32_t)>(
              iface->get_subscriptions)(&s, sizeof(s));
      result.sub_end.supported = s.supported_mask;
      result.sub_end.active = s.active_mask;
      result.sub_end.requested = s.requested_mask;
      result.sub_end.activation_frame = s.activation_frame;
    }
    if (iface && g_sub_mask >= 0) {
      if (iface->poll_audio_events) {
        std::vector<uint8_t> buffer(80u * 1024u);
        result.sub_poll_status = reinterpret_cast<int32_t(__cdecl *)(
            void *, uint32_t, uint32_t *)>(iface->poll_audio_events)(
            buffer.data(), 1024, &result.sub_poll_count);
      }
      if (iface->get_audio_transport_stats) {
        std::vector<uint8_t> stats(256);
        result.sub_stats_status = reinterpret_cast<int32_t(__cdecl *)(
            void *, uint32_t)>(iface->get_audio_transport_stats)(
            stats.data(), 32);
      }
    }
    if (iface && dump_regions) {
      for (uint32_t id = 1; id < TESSERA_OBS_REGION_COUNT; ++id) {
        RegionDump d;
        d.id = id;
        tessera_obs_region_info_v1 info{};
        d.query_status = iface->query_region(id, &info, sizeof(info));
        d.byte_size = info.byte_size;
        d.access = info.access_flags;
        d.element_size = info.element_size;
        d.capacity = info.capacity;
        d.data_version = info.data_version;
        d.legacy_id = info.legacy_memory_id;
        if (d.query_status == 0 &&
            (info.access_flags & TESSERA_OBS_REGION_ACCESS_READ)) {
          std::vector<uint8_t> buf(info.byte_size);
          uint64_t generation = 0;
          d.read_status = iface->read_region(id, 0, buf.data(), info.byte_size,
                                             UINT64_MAX, &generation);
          if (d.read_status == 0) {
            d.bytes_read = info.byte_size;
            d.hash = fnv1a(buf.data(), buf.size(), 1469598103934665603ull);
            d.bytes = std::move(buf);
          }
        }
        result.regions.push_back(d);
      }
    }
    if (!g_recompose_out.empty()) {
      // TP-205.1: status, width, height (int32, uint32, uint32), then the
      // width x height RGB565 pixels recompose_frame returned.
      std::vector<uint16_t> pixels((std::max)(g_recompose_capacity, 1u));
      int32_t status = -8; // AYTHER_STATUS_UNSUPPORTED: no recompose_frame
      uint32_t width = 0, height = 0;
      if (iface && iface->recompose_frame)
        status = iface->recompose_frame(pixels.data(), g_recompose_capacity,
                                        g_recompose_flags, &width, &height);
      std::ofstream out(g_recompose_out, std::ios::binary);
      out.write(reinterpret_cast<const char *>(&status), 4);
      out.write(reinterpret_cast<const char *>(&width), 4);
      out.write(reinterpret_cast<const char *>(&height), 4);
      if (status == 0)
        out.write(reinterpret_cast<const char *>(pixels.data()),
                  static_cast<std::streamsize>(
                      (std::min<size_t>)(size_t{width} * height,
                                         pixels.size()) *
                      2));
    }
    if (!g_multilayer_out.empty()) {
      // TP-205.2: status, width, height, then plane A, plane B, window,
      // sprites and composite, width x height RGB565 each.
      const uint32_t capacity = g_recompose_capacity;
      std::vector<std::vector<uint16_t>> layers(
          5, std::vector<uint16_t>((std::max)(capacity, 1u)));
      int32_t status = -8; // AYTHER_STATUS_UNSUPPORTED
      uint32_t width = 0, height = 0;
      if (iface &&
          iface->struct_size >= offsetof(AytherInterface, recompose_multilayer) +
                                    sizeof(void *) &&
          iface->recompose_multilayer)
        status = iface->recompose_multilayer(
            layers[0].data(), layers[1].data(), layers[2].data(),
            layers[3].data(), layers[4].data(), capacity, g_recompose_flags,
            &width, &height);
      std::ofstream out(g_multilayer_out, std::ios::binary);
      out.write(reinterpret_cast<const char *>(&status), 4);
      out.write(reinterpret_cast<const char *>(&width), 4);
      out.write(reinterpret_cast<const char *>(&height), 4);
      if (status == 0)
        for (const auto &layer : layers)
          out.write(reinterpret_cast<const char *>(layer.data()),
                    static_cast<std::streamsize>(
                        (std::min<size_t>)(size_t{width} * height,
                                           layer.size()) *
                        2));
    }
    using MemSize = size_t(__cdecl *)(unsigned);
    using MemData = void *(__cdecl *)(unsigned);
    MemSize mem_size = nullptr;
    MemData mem_data = nullptr;
    if (sym(core, "retro_get_memory_size", mem_size) &&
        sym(core, "retro_get_memory_data", mem_data)) {
      const auto grab = [&](unsigned id, std::vector<uint8_t> &out) {
        const size_t n = mem_size(id);
        const auto *p = static_cast<const uint8_t *>(mem_data(id));
        if (n != 0 && p != nullptr)
          out.assign(p, p + n);
      };
      grab(0, result.save_ram);
      grab(2, result.work_ram);
    }
    unload();
  }
  deinit();
  g_result = nullptr;
  FreeLibrary(core);
  return result;
}

} // namespace diffbench
