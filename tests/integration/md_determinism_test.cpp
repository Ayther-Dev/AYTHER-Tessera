// TP-110.3 (RF-4.2): the native Mega Drive is deterministic. The same ROM and
// the same inputs give the same digest of every frame in two different
// processes (so nothing depends on addresses, uninitialised memory or time).
//
// The test runs itself: the parent starts two children (`--child <name>
// <frames>`), each prints one line with the chain of per frame digests, and the
// parent compares the lines. Synthetic ROMs always (3 000 frames); commercial
// ROMs when TESSERA_DETERMINISM_ROMS lists them (separated by '|').
#define DOCTEST_CONFIG_IMPLEMENT
#include "md/region.hpp"
#include "md/system.hpp"
#include "support/md_digest.hpp"
#include "support/md_testrom.hpp"
#include "support/md_z80_testrom.hpp"
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace tessera::md;

namespace {
using Rom = std::vector<std::uint8_t>;
std::string g_self;

const std::map<std::string, std::function<Rom()>> &synthetic() {
  using namespace tessera::testrom;
  static const std::map<std::string, std::function<Rom()>> roms{
      {"vint_counter", [] { return vint_counter_rom(); }},
      {"pad_frames", [] { return pad_frames_rom(); }},
      {"fm_psg_audible", [] { return fm_psg_sweep_rom(5); }},
      {"fm_psg_sweep", [] { return fm_psg_sweep_rom(0); }},
      {"dma_fifo", [] { return dma_fifo_rom(); }},
      {"hint_counter", [] { return hint_counter_rom(9); }},
      {"z80_int", [] { return z80_int_rom(0); }},
      {"bus_sweep", [] { return bus_sweep_rom(); }},
      {"sprite_limits", [] { return sprite_limits_rom(sprite_variant(1)); }},
      {"modes_sweep", [] { return modes_sweep_rom(modes_variant(1)); }},
      {"region_detect_pal", [] { return region_detect_rom("E"); }},
  };
  return roms;
}

Rom load_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return Rom((std::istreambuf_iterator<char>(in)),
             std::istreambuf_iterator<char>());
}

/// Runs `frames` frames with a moving pad and returns the chain of digests.
std::uint64_t run_chain(const Rom &rom, unsigned frames, bool observe = false) {
  auto config = config_for_rom(rom);
  config.pads = {PadType::six_button, PadType::six_button};
  auto system = std::make_unique<System>(rom, config);
  system->reset();
  tessera::testsupport::Fnv chain;
  for (unsigned i = 0; i < frames; ++i) {
    system->set_pad(0, static_cast<std::uint16_t>(0x100 + (i * 37) % 0xFF));
    system->set_pad(1, static_cast<std::uint16_t>((i * 11) % 0xFFF));
    system->run_frame();
    if (observe) {
      // Everything an observer reads between frames (TP-110.3): the audio of
      // the frame, the original video and a full checkpoint export. None of it
      // may change what the machine computes.
      volatile std::size_t sink = system->audio().size();
      sink = sink + system->renderer().frame().size();
      if (i % 40 == 0)
        sink = sink + system->save_state().size();
      (void)sink;
    }
    chain.add(tessera::testsupport::digest(*system));
  }
  return chain.h;
}

/// Child side: `name` is a synthetic ROM or a path.
int child(const std::string &name, unsigned frames, bool observe) {
  Rom rom;
  if (const auto it = synthetic().find(name); it != synthetic().end())
    rom = it->second();
  else
    rom = load_file(name);
  if (rom.empty()) {
    std::fprintf(stderr, "no ROM: %s\n", name.c_str());
    return 2;
  }
  std::printf("chain %016llx\n",
              static_cast<unsigned long long>(run_chain(rom, frames, observe)));
  return 0;
}

#ifdef _WIN32
#define TESSERA_POPEN _popen
#define TESSERA_PCLOSE _pclose
#else
#define TESSERA_POPEN popen
#define TESSERA_PCLOSE pclose
#endif

/// Parent side: the output line of one child process.
std::string spawn(const std::string &name, unsigned frames,
                  bool observe = false) {
  std::string command = "\"" + g_self + "\" --child \"" + name + "\" " +
                        std::to_string(frames) + (observe ? " observe" : "");
#ifdef _WIN32
  command = "\"" + command + "\""; // cmd.exe strips the outer pair
#endif
  std::FILE *pipe = TESSERA_POPEN(command.c_str(), "r");
  REQUIRE_MESSAGE(pipe != nullptr, command);
  std::string line;
  char buffer[128];
  while (std::fgets(buffer, sizeof buffer, pipe) != nullptr)
    line += buffer;
  const int status = TESSERA_PCLOSE(pipe);
  CHECK_MESSAGE(status == 0, name);
  return line;
}

void check_two_processes(const std::string &name, unsigned frames) {
  CAPTURE(name);
  const auto first = spawn(name, frames);
  const auto second = spawn(name, frames);
  CHECK(first.rfind("chain ", 0) == 0);
  CHECK(first == second);
  // With an observer reading every frame the chain is the same one.
  const auto watched = spawn(name, frames, true);
  CHECK(watched == first);
}
} // namespace

TEST_CASE("two processes give the same frames on synthetic ROMs") {
  for (const auto &entry : synthetic())
    check_two_processes(entry.first, 3000);
}

TEST_CASE("two runs in one process agree, and a different input differs") {
  const auto rom = tessera::testrom::pad_frames_rom();
  CHECK(run_chain(rom, 120) == run_chain(rom, 120));
  CHECK(run_chain(rom, 120) != run_chain(rom, 121));
}

TEST_CASE("commercial ROMs from TESSERA_DETERMINISM_ROMS") {
  const char *list = std::getenv("TESSERA_DETERMINISM_ROMS");
  if (list == nullptr || *list == '\0')
    return;
  std::stringstream paths(list);
  for (std::string path; std::getline(paths, path, '|');)
    if (!path.empty())
      check_two_processes(path, 3000);
}

int main(int argc, char **argv) {
  g_self = argv[0];
  if ((argc == 4 || argc == 5) && std::string(argv[1]) == "--child")
    return child(argv[2], static_cast<unsigned>(std::atoi(argv[3])),
                 argc == 5 && std::string(argv[4]) == "observe");
  doctest::Context context(argc, argv);
  return context.run();
}
