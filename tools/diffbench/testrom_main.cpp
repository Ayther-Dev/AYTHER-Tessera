// Writes the synthetic Mega Drive test ROMs (spec 008, TP-101.10).
//   tessera_md_testrom bus-sweep|ssf2-sweep|vint-counter <out.bin>
#include "support/md_testrom.hpp"

#include <cstdio>
#include <cstdlib>
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
  else if (std::strcmp(argv[1], "hint-counter") == 0)
    rom = tessera::testrom::hint_counter_rom(9);
  else if (std::strcmp(argv[1], "hint-every-line") == 0)
    rom = tessera::testrom::hint_counter_rom(0);
  else if (std::strcmp(argv[1], "hv-pal-v28") == 0)
    rom = tessera::testrom::hv_pal_probe_rom(0x00);
  else if (std::strcmp(argv[1], "hv-pal-v30") == 0)
    rom = tessera::testrom::hv_pal_probe_rom(0x08);
  else if (std::strcmp(argv[1], "hv-v28-h32") == 0)
    rom = tessera::testrom::hv_probe_rom(0x00, 0x00);
  else if (std::strcmp(argv[1], "hv-v28-h40") == 0)
    rom = tessera::testrom::hv_probe_rom(0x00, 0x81);
  else if (std::strcmp(argv[1], "hv-v30-h32") == 0)
    rom = tessera::testrom::hv_probe_rom(0x08, 0x00);
  else if (std::strcmp(argv[1], "hv-h32-h") == 0)
    rom = tessera::testrom::hv_h_probe_rom(0x00);
  else if (std::strcmp(argv[1], "hv-h40-h") == 0)
    rom = tessera::testrom::hv_h_probe_rom(0x81);
  else if (std::strcmp(argv[1], "mode4-pbc") == 0)
    rom = tessera::testrom::mode4_pbc_rom();
  else if (std::strncmp(argv[1], "palette-sweep-", 14) == 0)
    rom = tessera::testrom::palette_sweep_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 14, nullptr, 10)));
  else if (std::strncmp(argv[1], "modes-sweep-", 12) == 0)
    rom = tessera::testrom::modes_sweep_rom(tessera::testrom::modes_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 12, nullptr, 10))));
  else if (std::strncmp(argv[1], "sprite-limits-", 14) == 0)
    rom = tessera::testrom::sprite_limits_rom(tessera::testrom::sprite_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 14, nullptr, 10))));
  else if (std::strncmp(argv[1], "mode4-scene-", 12) == 0) {
    rom = tessera::testrom::mode4_scene_rom(tessera::testrom::mode4_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 12, nullptr, 10))));
  } else if (std::strcmp(argv[1], "dma-fifo") == 0)
    rom = tessera::testrom::dma_fifo_rom();
  else if (std::strncmp(argv[1], "dma-timing-", 11) == 0) {
    const char *v = argv[1] + 11; // <target>-<d|b>-<h32|h40>-<a|v>
    rom = tessera::testrom::dma_timing_rom(static_cast<unsigned>(v[0] - '0'),
                                           v[2] == 'd', v[5] == '4', v[8] == 'v');
  } else if (std::strcmp(argv[1], "cpu-z80area") == 0)
    rom = tessera::testrom::cpu_timing_rom(tessera::testrom::cpu_z80area_groups());
  else if (std::strcmp(argv[1], "cpu-drift-long") == 0)
    rom = tessera::testrom::cpu_drift_long_rom();
  else if (std::strcmp(argv[1], "cpu-muldiv") == 0)
    rom = tessera::testrom::cpu_timing_rom(tessera::testrom::cpu_muldiv_groups());
  else if (std::strcmp(argv[1], "cpu-timing") == 0)
    rom = tessera::testrom::cpu_timing_rom();
  else if (std::strcmp(argv[1], "z80-busreq") == 0)
    rom = tessera::testrom::z80_busreq_rom();
  else if (std::strcmp(argv[1], "cpu-drift") == 0)
    rom = tessera::testrom::cpu_drift_rom(true);
  else if (std::strncmp(argv[1], "fifo-timing-", 12) == 0) {
    // fifo-timing-<target 0..2>-<d|b>-<h32|h40>-<a|v>: display on or blanked
    const char *v = argv[1] + 12;
    rom = tessera::testrom::fifo_timing_rom(
        static_cast<unsigned>(v[0] - '0'), v[2] == 'd', v[5] == '4',
        v[8] == 'v', v[9] == 'r',
        (v[9] == 'n' || v[10] == 'n') ? 240U : 64U, v[9] == 'l');
  } else
    return 2;
  std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(rom.data()),
            static_cast<std::streamsize>(rom.size()));
  out.close();
  return out.good() ? 0 : 1;
}
