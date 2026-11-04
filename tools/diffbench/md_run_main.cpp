// Runs a ROM on the native Mega Drive machine and writes its work RAM
// (spec 008, TP-101.9). The differential bench compares that file with the one
// gpgx produces (`tessera_diffbench --libretro ... --work-ram-out`).
//   tessera_md_run --rom <file> [--frames N] [--region j|u|e] --work-ram-out
//   <file>
#include "md/system.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  std::string rom_path, out_path, srm_path;
  unsigned frames = 600;
  tessera::md::Region region = tessera::md::Region::usa;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool has_value = i + 1 < argc;
    if (arg == "--rom" && has_value) {
      rom_path = argv[++i];
    } else if (arg == "--work-ram-out" && has_value) {
      out_path = argv[++i];
    } else if (arg == "--save-ram-out" && has_value) {
      srm_path = argv[++i];
    } else if (arg == "--frames" && has_value) {
      frames = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    } else if (arg == "--region" && has_value) {
      const char r = argv[++i][0];
      region = r == 'j'   ? tessera::md::Region::japan
               : r == 'e' ? tessera::md::Region::europe
                          : tessera::md::Region::usa;
    } else {
      std::fputs("usage: tessera_md_run --rom <file> [--frames N] "
                 "[--region j|u|e] --work-ram-out <file>\n",
                 stderr);
      return 2;
    }
  }
  if (rom_path.empty() || out_path.empty()) {
    std::fputs("--rom and --work-ram-out are required\n", stderr);
    return 2;
  }
  std::ifstream in(rom_path, std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "cannot open %s\n", rom_path.c_str());
    return 1;
  }
  std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  tessera::md::System system(std::move(rom),
                             tessera::md::MachineConfig::for_region(region));
  system.reset();
  for (unsigned frame = 0; frame < frames; ++frame)
    system.run_frame();
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  const auto ram = system.work_ram();
  out.write(reinterpret_cast<const char *>(ram.data()),
            static_cast<std::streamsize>(ram.size()));
  out.close();
  if (!srm_path.empty()) {
    const auto srm = system.sram().to_srm();
    std::ofstream srm_out(srm_path, std::ios::binary | std::ios::trunc);
    srm_out.write(reinterpret_cast<const char *>(srm.data()),
                  static_cast<std::streamsize>(srm.size()));
  }
  std::printf("frames=%u work_ram=%zu\n", frames, ram.size());
  return out.good() ? 0 : 1;
}
