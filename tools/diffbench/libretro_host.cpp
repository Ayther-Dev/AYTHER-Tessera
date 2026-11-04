#include "libretro_host.hpp"

#include <windows.h>

#include "tessera/tessera_observation.h"

#include <cstdlib>
#include <fstream>
#include <iterator>

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
  void *write_control;
  void *capture_snapshot;
  void *recompose_frame;
  uint32_t audio_event_size, audio_transport_stats_size;
  void *poll_audio_events;
  void *get_audio_transport_stats;
  uint32_t subscription_state_size, reserved1;
  void *get_subscriptions;
  int32_t(__cdecl *set_subscriptions)(uint32_t);
};

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
  case 10:                                             // SET_PIXEL_FORMAT
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

void __cdecl video(const void *data, unsigned w, unsigned h, size_t pitch) {
  if (!g_result)
    return;
  uint64_t hash = 1469598103934665603ull;
  if (data) {
    const auto *rows = static_cast<const uint8_t *>(data);
    for (unsigned y = 0; y < h; ++y)
      hash = fnv1a(rows + y * pitch, size_t{w} * 4, hash);
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
  for (size_t i = 0; i < frames * 2; ++i)
    note_peak(data[i]);
  return frames;
}
void __cdecl poll() { ++g_frame; }
// Controles de la API (up down left right A B C start) -> ids libretro de gpgx
// (A=JOYPAD_Y, B=JOYPAD_B, C=JOYPAD_A, start=START).
int16_t __cdecl input(unsigned port, unsigned device, unsigned, unsigned id) {
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

RunResult run_libretro(const std::string &core_path,
                       const std::string &rom_path, unsigned frames,
                       bool dump_regions,
                       const std::vector<TraceFrame> *trace) {
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
    using GetIface = const AytherInterface *(__cdecl *)(uint32_t);
    GetIface get_iface = nullptr;
    const AytherInterface *iface = nullptr;
    if (dump_regions && sym(core, "ayther_get_interface", get_iface))
      iface = get_iface(TESSERA_OBS_ABI_VERSION_1_10);
    if (dump_regions && !iface)
      result.error = "el core no ofrece la interfaz AYTHER 1.10";
    if (iface)
      iface->set_subscriptions(TESSERA_OBS_SUB_ALL);
    for (unsigned i = 0; i < frames && result.error.empty(); ++i)
      run();
    if (iface) {
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
          }
        }
        result.regions.push_back(d);
      }
    }
    unload();
  }
  deinit();
  g_result = nullptr;
  FreeLibrary(core);
  return result;
}

} // namespace diffbench
