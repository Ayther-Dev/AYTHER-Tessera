// Runs a ROM on the native Mega Drive machine and writes its work RAM
// (spec 008, TP-101.9). The differential bench compares that file with the one
// gpgx produces (`tessera_diffbench --libretro ... --work-ram-out`).
//   tessera_md_run --rom <file> [--frames N] [--region a|u|e|j|p] --work-ram-out
//   <file>
#include "md/region.hpp"
#include "md/system.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  std::string rom_path, out_path, srm_path, frame_path, palette_path, vram_path,
      regs_path, z80_path, poke_spec, audio_path;
  unsigned long pad_mask[2] = {0, 0};
  int pad_type = 3;
  unsigned frames = 600;
  tessera::md::RegionChoice region = tessera::md::RegionChoice::automatic;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool has_value = i + 1 < argc;
    if (arg == "--rom" && has_value) {
      rom_path = argv[++i];
    } else if (arg == "--work-ram-out" && has_value) {
      out_path = argv[++i];
    } else if (arg == "--save-ram-out" && has_value) {
      srm_path = argv[++i];
    } else if (arg == "--palette-out" && has_value) {
      palette_path = argv[++i];
    } else if (arg == "--vram-out" && has_value) {
      vram_path = argv[++i];
    } else if (arg == "--z80-ram-out" && has_value) {
      z80_path = argv[++i];
    } else if (arg == "--pad-mask" && has_value) {
      pad_mask[0] = std::strtoul(argv[++i], nullptr, 16);
    } else if (arg == "--pad-mask2" && has_value) {
      pad_mask[1] = std::strtoul(argv[++i], nullptr, 16);
    } else if (arg == "--pad-type" && has_value) {
      pad_type = std::atoi(argv[++i]);
    } else if (arg == "--audio-writes-out" && has_value) {
      audio_path = argv[++i]; // 8 byte records of the last frame's log
    } else if (arg == "--poke-z80" && has_value) {
      poke_spec = argv[++i]; // <offset hex>:<bytes hex>:<frame>
    } else if (arg == "--regs-out" && has_value) {
      regs_path = argv[++i];
    } else if (arg == "--frame-out" && has_value) {
      frame_path = argv[++i];
    } else if (arg == "--frames" && has_value) {
      frames = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    } else if (arg == "--region" && has_value) {
      const char r = argv[++i][0];
      region = r == 'j'   ? tessera::md::RegionChoice::japan_ntsc
               : r == 'p' ? tessera::md::RegionChoice::japan_pal
               : r == 'e' ? tessera::md::RegionChoice::europe
               : r == 'u' ? tessera::md::RegionChoice::usa
                          : tessera::md::RegionChoice::automatic;
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
  auto config = tessera::md::config_for_rom(rom, region);
  config.pads.fill(pad_type == 6 ? tessera::md::PadType::six_button
                                 : tessera::md::PadType::three_button);
  tessera::md::System system(std::move(rom), config);
  system.reset();
  for (unsigned port = 0; port < 2; ++port)
    system.set_pad(port, static_cast<std::uint16_t>(pad_mask[port]));
  std::size_t poke_offset = 0;
  std::vector<std::uint8_t> poke_bytes;
  unsigned poke_frame = 0;
  if (!poke_spec.empty()) {
    const auto c1 = poke_spec.find(':');
    const auto c2 = poke_spec.find(':', c1 + 1);
    if (c1 == std::string::npos || c2 == std::string::npos ||
        (c2 - c1 - 1) % 2 != 0) {
      std::fputs("--poke-z80 <offset hex>:<bytes hex>:<frame>\n", stderr);
      return 2;
    }
    poke_offset = std::strtoul(poke_spec.c_str(), nullptr, 16);
    for (auto i = c1 + 1; i < c2; i += 2)
      poke_bytes.push_back(static_cast<std::uint8_t>(
          std::strtoul(poke_spec.substr(i, 2).c_str(), nullptr, 16)));
    poke_frame = static_cast<unsigned>(std::strtoul(poke_spec.c_str() + c2 + 1, nullptr, 10));
  }
  for (unsigned frame = 0; frame < frames; ++frame) {
    if (!poke_spec.empty() && frame == poke_frame &&
        !system.poke_z80_ram(poke_offset, poke_bytes)) {
      std::fputs("poke out of range\n", stderr);
      return 2;
    }
    system.run_frame();
  }
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  const auto ram = system.work_ram();
  out.write(reinterpret_cast<const char *>(ram.data()),
            static_cast<std::streamsize>(ram.size()));
  out.close();
  if (!palette_path.empty()) {
    const auto table = tessera::md::palette_565(system.vdp());
    std::ofstream po(palette_path, std::ios::binary | std::ios::trunc);
    po.write(reinterpret_cast<const char *>(table.data()),
             static_cast<std::streamsize>(table.size() * 2U));
  }
  if (!z80_path.empty()) {
    std::ofstream zo(z80_path, std::ios::binary | std::ios::trunc);
    const auto z = system.z80_ram();
    zo.write(reinterpret_cast<const char *>(z.data()),
             static_cast<std::streamsize>(z.size()));
  }
  if (!audio_path.empty()) {
    std::ofstream ao(audio_path, std::ios::binary | std::ios::trunc);
    for (const auto &w : system.audio_writes()) {
      const unsigned char rec[8] = {
          static_cast<unsigned char>(w.cycle),
          static_cast<unsigned char>(w.cycle >> 8),
          static_cast<unsigned char>(w.cycle >> 16),
          static_cast<unsigned char>(w.cycle >> 24),
          static_cast<unsigned char>(w.addr),
          static_cast<unsigned char>(w.addr >> 8),
          w.data, w.chip};
      ao.write(reinterpret_cast<const char *>(rec), 8);
    }
  }
  if (!regs_path.empty()) {
    std::ofstream ro(regs_path, std::ios::binary | std::ios::trunc);
    for (unsigned r = 0; r < 32; ++r)
      ro.put(static_cast<char>(system.vdp().reg(r)));
  }
  if (!vram_path.empty()) {
    std::ofstream vo(vram_path, std::ios::binary | std::ios::trunc);
    const auto &vram = system.vdp().vram();
    vo.write(reinterpret_cast<const char *>(vram.data()),
             static_cast<std::streamsize>(vram.size()));
  }
  if (!frame_path.empty()) {
    // Same layout as the bench's oracle dump: width, height, pitch (u32 LE)
    // and the rows; here RGBA8, one u32 per pixel (R in the low byte).
    const auto size = system.renderer().size();
    const std::uint32_t head[3] = {size.width, size.height, size.width * 4U};
    std::ofstream fo(frame_path, std::ios::binary | std::ios::trunc);
    fo.write(reinterpret_cast<const char *>(head), sizeof head);
    const auto &frame = system.renderer().frame();
    fo.write(reinterpret_cast<const char *>(frame.data()),
             static_cast<std::streamsize>(frame.size() * 4U));
  }
  if (!srm_path.empty()) {
    const auto srm = system.sram().to_srm();
    std::ofstream srm_out(srm_path, std::ios::binary | std::ios::trunc);
    srm_out.write(reinterpret_cast<const char *>(srm.data()),
                  static_cast<std::streamsize>(srm.size()));
  }
  std::printf("frames=%u work_ram=%zu\n", frames, ram.size());
  return out.good() ? 0 : 1;
}
