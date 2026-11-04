#include "libretro_host.hpp"

#include <windows.h>

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

RunResult *g_result = nullptr;

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
void __cdecl audio(int16_t, int16_t) {}
size_t __cdecl audio_batch(const int16_t *, size_t frames) { return frames; }
void __cdecl poll() {}
int16_t __cdecl input(unsigned, unsigned, unsigned, unsigned) { return 0; }

template <class F> bool sym(HMODULE m, const char *name, F &out) {
  out = reinterpret_cast<F>(GetProcAddress(m, name));
  return out != nullptr;
}

} // namespace

RunResult run_libretro(const std::string &core_path,
                       const std::string &rom_path, unsigned frames) {
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
    for (unsigned i = 0; i < frames; ++i)
      run();
    unload();
  }
  deinit();
  g_result = nullptr;
  FreeLibrary(core);
  return result;
}

} // namespace diffbench
