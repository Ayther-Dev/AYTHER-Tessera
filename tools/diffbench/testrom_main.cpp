// Writes the synthetic Mega Drive test ROMs (spec 008, TP-101.10).
//   tessera_md_testrom bus-sweep|ssf2-sweep|vint-counter <out.bin>
#include "support/md_delta_testrom.hpp"
#include "support/md_limits_testrom.hpp"
#include "support/md_testrom.hpp"
#include "support/md_z80_testrom.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

int main(int argc, char **argv) {
  if (argc != 3 && argc != 6) {
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
  else if (std::strncmp(argv[1], "region-detect-", 14) == 0) {
    // region-detect-<country hex, up to 16 bytes>[-<checksum hex>]; the rest
    // of the field is spaces.
    std::array<std::uint8_t, 16> country;
    country.fill(' ');
    const char *v = argv[1] + 14;
    for (std::size_t i = 0; i < 16 && std::isxdigit(v[0]) && std::isxdigit(v[1]);
         ++i, v += 2) {
      char pair[3] = {v[0], v[1], 0};
      country[i] = static_cast<std::uint8_t>(std::strtoul(pair, nullptr, 16));
    }
    const auto checksum =
        *v == '-' ? static_cast<std::uint16_t>(std::strtoul(v + 1, nullptr, 16))
                  : std::uint16_t{0};
    rom = tessera::testrom::region_detect_rom(country, checksum);
  } else if (std::strcmp(argv[1], "pad-frames") == 0)
    rom = tessera::testrom::pad_frames_rom();
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
  else if (std::strcmp(argv[1], "audio-flood") == 0)
    rom = tessera::testrom::audio_flood_rom();
  else if (std::strcmp(argv[1], "mode4-vram-0") == 0)
    rom = tessera::testrom::mode4_vram_rom(false);
  else if (std::strcmp(argv[1], "mode4-vram-1") == 0)
    rom = tessera::testrom::mode4_vram_rom(true);
  else if (std::strcmp(argv[1], "mode4-pbc") == 0)
    rom = tessera::testrom::mode4_pbc_rom();
  else if (std::strncmp(argv[1], "palette-sweep-", 14) == 0)
    rom = tessera::testrom::palette_sweep_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 14, nullptr, 10)));
  else if (std::strncmp(argv[1], "modes-hv-", 9) == 0) {
    // modes-hv-<variant>-<words between HV reads> (TP-209.1 timing probe)
    char *end = nullptr;
    const auto id = std::strtoul(argv[1] + 9, &end, 10);
    // [-<nops before the display write>]
    const auto every = *end == '-' ? std::strtoul(end + 1, &end, 10) : 256UL;
    // [-<nops before the CRAM data>]
    const auto nops = *end == '-' ? std::strtoul(end + 1, &end, 10) : 0UL;
    // [-d: display on before the CRAM data | -m: Mode 4 before the display]
    const auto cram = *end == '-' ? std::strtoul(end + 1, &end, 10) : 0UL;
    rom = tessera::testrom::modes_sweep_rom(
        tessera::testrom::modes_variant(static_cast<unsigned>(id)),
        static_cast<unsigned>(every), static_cast<unsigned>(nops),
        static_cast<unsigned>(cram), std::strcmp(end, "-d") == 0,
        std::strcmp(end, "-m") == 0);
  }
  else if (std::strncmp(argv[1], "modes-sweep-", 12) == 0)
    rom = tessera::testrom::modes_sweep_rom(tessera::testrom::modes_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 12, nullptr, 10))));
  else if (std::strncmp(argv[1], "sprite-limits-", 14) == 0)
    rom = tessera::testrom::sprite_limits_rom(tessera::testrom::sprite_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 14, nullptr, 10))));
  else if (std::strncmp(argv[1], "raster-bands-", 13) == 0)
    rom = tessera::testrom::raster_bands_rom(tessera::testrom::raster_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 13, nullptr, 10))));
  else if (std::strncmp(argv[1], "mode4-scene-", 12) == 0) {
    rom = tessera::testrom::mode4_scene_rom(tessera::testrom::mode4_variant(
        static_cast<unsigned>(std::strtoul(argv[1] + 12, nullptr, 10))));
  } else if (std::strncmp(argv[1], "delta-probe-", 12) == 0) {
    rom = tessera::testrom::delta_probe_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 12, nullptr, 10)));
  } else if (std::strcmp(argv[1], "dma-fifo") == 0)
    rom = tessera::testrom::dma_fifo_rom();
  else if (std::strncmp(argv[1], "dma-timing-", 11) == 0) {
    const char *v = argv[1] + 11; // <target>-<d|b>-<h32|h40>-<a|v>
    rom = tessera::testrom::dma_timing_rom(static_cast<unsigned>(v[0] - '0'),
                                           v[2] == 'd', v[5] == '4', v[8] == 'v');
  } else if (std::strncmp(argv[1], "dmafill-timing-", 15) == 0) {
    const char *v = argv[1] + 15; // <f|c>-<d|b>-<h32|h40>
    rom = tessera::testrom::dma_fill_timing_rom(v[0] == 'c', v[2] == 'd',
                                                v[5] == '4');
  } else if (std::strcmp(argv[1], "cpu-z80area") == 0)
    rom = tessera::testrom::cpu_timing_rom(tessera::testrom::cpu_z80area_groups());
  else if (std::strcmp(argv[1], "cpu-drift-long") == 0)
    rom = tessera::testrom::cpu_drift_long_rom();
  else if (std::strcmp(argv[1], "cpu-muldiv") == 0)
    rom = tessera::testrom::cpu_timing_rom(tessera::testrom::cpu_muldiv_groups());
  else if (std::strcmp(argv[1], "cpu-timing") == 0)
    rom = tessera::testrom::cpu_timing_rom();
  else if (std::strncmp(argv[1], "z80-bank-", 9) == 0)
    rom = tessera::testrom::z80_bank_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 9, nullptr, 10)));
  else if (std::strcmp(argv[1], "z80-map") == 0)
    rom = tessera::testrom::z80_map_rom();
  else if (std::strncmp(argv[1], "z80-contention-", 15) == 0)
    rom = tessera::testrom::z80_contention_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 15, nullptr, 10)));
  else if (std::strncmp(argv[1], "z80-int-", 8) == 0)
    rom = tessera::testrom::z80_int_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 8, nullptr, 10)));
  else if (std::strcmp(argv[1], "z80-clock") == 0)
    rom = tessera::testrom::z80_clock_rom(false);
  else if (std::strcmp(argv[1], "z80-clock-bank") == 0)
    rom = tessera::testrom::z80_clock_rom(true);
  else if (std::strncmp(argv[1], "ym-busy-", 8) == 0)
    rom = tessera::testrom::ym_busy_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 8, nullptr, 10)));
  else if (std::strncmp(argv[1], "pad-latch-", 10) == 0)
    rom = argc > 5 ? tessera::testrom::pad_latch_rom(
                         static_cast<unsigned>(std::strtoul(argv[1] + 10, nullptr, 10)),
                         static_cast<unsigned>(std::strtoul(argv[3], nullptr, 10)),
                         static_cast<unsigned>(std::strtoul(argv[4], nullptr, 10)),
                         static_cast<unsigned>(std::strtoul(argv[5], nullptr, 10)))
                   : tessera::testrom::pad_latch_rom(
                         static_cast<unsigned>(std::strtoul(argv[1] + 10, nullptr, 10)));
  else if (std::strncmp(argv[1], "fm-rom-z80-", 11) == 0)
    rom = tessera::testrom::fm_rom_z80_rom(
        static_cast<unsigned>(std::atoi(argv[1] + 11)));
  else if (std::strncmp(argv[1], "fm-busy-z80-", 12) == 0)
    rom = tessera::testrom::fm_busy_z80_rom(
        static_cast<unsigned>(std::atoi(argv[1] + 12)));
  else if (std::strcmp(argv[1], "fm-psg-z80") == 0)
    rom = tessera::testrom::fm_psg_z80_rom();
  else if (std::strncmp(argv[1], "fm-psg-", 7) == 0)
    rom = tessera::testrom::fm_psg_sweep_rom(
        static_cast<unsigned>(std::strtoul(argv[1] + 7, nullptr, 10)),
        std::strstr(argv[1], "-once") != nullptr);
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
  } else if (std::strncmp(argv[1], "fifo-status-", 12) == 0 &&
             std::strlen(argv[1]) == 21) {
    // fifo-status-<target 0|1>-<d|b>-<h32|h40>-<a|v>
    const char *v = argv[1] + 12;
    rom = tessera::testrom::fifo_status_rom(static_cast<unsigned>(v[0] - '0'),
                                            v[2] == 'd', v[5] == '4',
                                            v[8] == 'v');
  } else
    return 2;
  std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(rom.data()),
            static_cast<std::streamsize>(rom.size()));
  out.close();
  return out.good() ? 0 : 1;
}
