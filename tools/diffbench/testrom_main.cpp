// Writes the synthetic Mega Drive test ROMs (spec 008, TP-101.10).
//   tessera_md_testrom bus-sweep|ssf2-sweep|vint-counter <out.bin>
#include "support/md_testrom.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::fputs("usage: tessera_md_testrom bus-sweep|ssf2-sweep|vint-counter "
               "<out.bin>\n",
               stderr);
    return 2;
  }
  std::vector<std::uint8_t> rom;
  if (std::strcmp(argv[1], "bus-sweep") == 0)
    rom = tessera::testrom::bus_sweep_rom();
  else if (std::strcmp(argv[1], "ssf2-sweep") == 0)
    rom = tessera::testrom::ssf2_sweep_rom();
  else if (std::strcmp(argv[1], "vint-counter") == 0)
    rom = tessera::testrom::vint_counter_rom();
  else if (std::strcmp(argv[1], "dma-fifo") == 0)
    rom = tessera::testrom::dma_fifo_rom();
  else
    return 2;
  std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(rom.data()),
            static_cast<std::streamsize>(rom.size()));
  out.close();
  return out.good() ? 0 : 1;
}
