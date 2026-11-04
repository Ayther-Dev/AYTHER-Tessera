// Writes the synthetic Mega Drive test ROMs (spec 008, TP-101.10).
//   tessera_md_testrom bus-sweep <out.bin>
#include "support/md_testrom.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

int main(int argc, char **argv) {
  if (argc != 3 || std::strcmp(argv[1], "bus-sweep") != 0) {
    std::fputs("usage: tessera_md_testrom bus-sweep <out.bin>\n", stderr);
    return 2;
  }
  const auto rom = tessera::testrom::bus_sweep_rom();
  std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(rom.data()),
            static_cast<std::streamsize>(rom.size()));
  out.close();
  return out.good() ? 0 : 1;
}
