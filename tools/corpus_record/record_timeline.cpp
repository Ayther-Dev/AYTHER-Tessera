// record_timeline — grabador de partidas del corpus (spec 008, TP-004.5).
//
// Abre gpgx por libretro con el Engine, arranca en frío, toma el estado
// inicial (record_start antes del primer frame) y juega un guion de mando
// ("timeline" del replay QA, spec 006) mientras graba. Escribe un .arp.
//
//   record_timeline <core.dll> <rom> <guion.timeline> <salida.arp> <id>
//                   [--snap <frame>:<archivo.ppm>]...   (pantalla en ese frame)
//
// Gramática (subconjunto del Runtime, tools/audio_qa/pad_timeline.h):
//   start power_on | wait <dur> | press <botones> [<dur>] | hold <botones> <dur>
//   | down <botones> | up <botones>;  botones Genesis unidos con '+';
//   <dur> = <decimal>s | <decimal>ms | <entero>f; '#' abre un comentario.
// Los bits son los de gpgx: A=0x0002 B=0x0001 C=0x0100 X=0x0400 Y=0x0200
// Z=0x0800 start=0x0008 mode=0x0004 up/down/left/right=0x10/0x20/0x40/0x80.
#include "ayther/ayther_recording.h"
#include "ayther/ayther_session.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

const std::map<std::string, std::uint16_t> kButtons = {
    {"a", 0x0002}, {"b", 0x0001}, {"c", 0x0100},     {"x", 0x0400},
    {"y", 0x0200}, {"z", 0x0800}, {"start", 0x0008}, {"mode", 0x0004},
    {"up", 0x0010}, {"down", 0x0020}, {"left", 0x0040}, {"right", 0x0080}};
constexpr std::uint32_t kDefaultPressFrames = 4;

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool parse_buttons(const std::string &text, std::uint16_t &mask) {
  mask = 0;
  std::stringstream in(lower(text));
  std::string name;
  while (std::getline(in, name, '+')) {
    const auto it = kButtons.find(name);
    if (it == kButtons.end())
      return false;
    mask |= it->second;
  }
  return mask != 0;
}

bool parse_frames(const std::string &text, double fps, std::uint32_t &frames) {
  const std::string t = lower(text);
  char *end = nullptr;
  const double value = std::strtod(t.c_str(), &end);
  const std::string unit = end;
  if (end == t.c_str() || value < 0)
    return false;
  if (unit == "f")
    frames = static_cast<std::uint32_t>(value);
  else if (unit == "s")
    frames = static_cast<std::uint32_t>(std::lround(value * fps));
  else if (unit == "ms")
    frames = static_cast<std::uint32_t>(std::lround(value * fps / 1000.0));
  else
    return false;
  return true;
}

// One mask per frame.
bool compile(const std::string &path, double fps,
             std::vector<std::uint16_t> &inputs) {
  std::ifstream file(path);
  if (!file) {
    std::fprintf(stderr, "no se puede abrir el guion %s\n", path.c_str());
    return false;
  }
  std::uint16_t held = 0;
  std::string line;
  unsigned number = 0;
  const auto fail = [&](const char *why) {
    std::fprintf(stderr, "%s:%u: %s\n", path.c_str(), number, why);
    return false;
  };
  while (std::getline(file, line)) {
    ++number;
    if (const auto hash = line.find('#'); hash != std::string::npos)
      line.erase(hash);
    std::stringstream words(line);
    std::string keyword, first, second;
    if (!(words >> keyword))
      continue;
    keyword = lower(keyword);
    words >> first >> second;
    std::uint32_t frames = 0;
    std::uint16_t mask = 0;
    if (keyword == "start") {
      if (lower(first) != "power_on")
        return fail("solo se admite «start power_on»");
    } else if (keyword == "wait") {
      if (!parse_frames(first, fps, frames))
        return fail("duración no válida");
      inputs.insert(inputs.end(), frames, held);
    } else if (keyword == "press" || keyword == "hold") {
      if (!parse_buttons(first, mask))
        return fail("botón desconocido");
      if (second.empty()) {
        if (keyword == "hold")
          return fail("hold exige duración");
        frames = kDefaultPressFrames;
      } else if (!parse_frames(second, fps, frames)) {
        return fail("duración no válida");
      }
      inputs.insert(inputs.end(), frames,
                    static_cast<std::uint16_t>(held | mask));
    } else if (keyword == "down" || keyword == "up") {
      if (!parse_buttons(first, mask))
        return fail("botón desconocido");
      held = keyword == "down" ? static_cast<std::uint16_t>(held | mask)
                               : static_cast<std::uint16_t>(held & ~mask);
    } else {
      return fail("instrucción desconocida");
    }
  }
  return !inputs.empty();
}

bool write_ppm(const ayther::FrameView &view, const std::string &path) {
  if (view.fb_pixels == nullptr || view.fb_width == 0)
    return false;
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << view.fb_width << ' ' << view.fb_height << "\n255\n";
  for (std::uint32_t y = 0; y < view.fb_height; ++y) {
    const auto *row =
        static_cast<const std::uint8_t *>(view.fb_pixels) + y * view.fb_pitch;
    for (std::uint32_t x = 0; x < view.fb_width; ++x) {
      std::uint8_t r, g, b;
      if (view.fb_format == 1) { // XRGB8888
        const std::uint32_t v = reinterpret_cast<const std::uint32_t *>(row)[x];
        r = v >> 16; g = v >> 8; b = v;
      } else if (view.fb_format == 2) { // RGB565
        const std::uint16_t v = reinterpret_cast<const std::uint16_t *>(row)[x];
        r = static_cast<std::uint8_t>((v >> 11 & 31) * 255 / 31);
        g = static_cast<std::uint8_t>((v >> 5 & 63) * 255 / 63);
        b = static_cast<std::uint8_t>((v & 31) * 255 / 31);
      } else { // 0RGB1555
        const std::uint16_t v = reinterpret_cast<const std::uint16_t *>(row)[x];
        r = static_cast<std::uint8_t>((v >> 10 & 31) * 255 / 31);
        g = static_cast<std::uint8_t>((v >> 5 & 31) * 255 / 31);
        b = static_cast<std::uint8_t>((v & 31) * 255 / 31);
      }
      out.put(static_cast<char>(r)).put(static_cast<char>(g)).put(
          static_cast<char>(b));
    }
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 6) {
    std::fprintf(stderr,
                 "uso: %s <core.dll> <rom> <guion.timeline> <salida.arp> <id>\n",
                 argv[0]);
    return 2;
  }
  ayther::AytherSession::Config cfg;
  cfg.core_path = argv[1];
  cfg.rom_path = argv[2];
  cfg.enable_audio = false;
  auto created = ayther::AytherSession::create(cfg);
  if (!created) {
    std::fprintf(stderr, "create: %s\n", created.error.message.c_str());
    return 1;
  }
  auto &session = *created;
  const double fps = session->timing_fps();

  std::vector<std::uint16_t> inputs;
  if (!compile(argv[3], fps, inputs))
    return 2;

  std::map<std::uint32_t, std::string> snaps;
  for (int i = 6; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--snap") {
      const std::string spec = argv[++i];
      const auto colon = spec.find(':');
      if (colon == std::string::npos)
        return 2;
      snaps[static_cast<std::uint32_t>(std::stoul(spec.substr(0, colon)))] =
          spec.substr(colon + 1);
    }
  }

  session->record_start(); // arranque en frío: antes del primer frame
  std::uint32_t frame = 0;
  for (const std::uint16_t mask : inputs) {
    session->set_input(0, mask);
    const ayther::FrameView &view = session->step();
    if (const auto it = snaps.find(frame); it != snaps.end())
      write_ppm(view, it->second);
    ++frame;
  }
  session->record_stop();
  ayther::AytherRecording take = session->take_recording();
  take.game_id = argv[5];
  take.name = argv[5];
  if (!take.save(argv[4])) {
    std::fprintf(stderr, "no se pudo guardar %s\n", argv[4]);
    return 1;
  }
  std::printf("frames=%u fps=%.4f\n", take.frame_count(), fps);
  return 0;
}
