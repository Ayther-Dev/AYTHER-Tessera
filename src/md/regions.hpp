// Memory regions of the observation contract (spec 008, TP-201.1, RF-3.1):
// the bytes the ABI 1.10 hands out for VRAM, CRAM, VDP_REGS, VSRAM and Z80_RAM,
// plus the console's work RAM, which the ABI leaves to the legacy memory map.
//
// Layouts are the ones the reference core shows through the bench (region
// table, src/observation/region_table.hpp): sizes and flags by id, the VRAM
// and the work RAM word-swapped (the logical byte `off` lives at `off ^ 1`,
// AYTHER_REGION_WORD_SWAPPED_LE), the colour RAM as native endian words of
// nine bits BBBGGGRRR and the scroll RAM as native endian 16 bit words. Reading
// never changes the machine.
#pragma once
#include "md/system.hpp"
#include "md/vdp_render.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tessera::md {

/// Ids of the ABI 1.10 regions served here.
inline constexpr std::uint32_t kRegionVram = 1;
inline constexpr std::uint32_t kRegionCram = 2;
inline constexpr std::uint32_t kRegionVdpRegs = 3;
inline constexpr std::uint32_t kRegionVsram = 4;
inline constexpr std::uint32_t kRegionParsedSprites = 13;
inline constexpr std::uint32_t kRegionParsedSpriteCount = 14;
inline constexpr std::uint32_t kRegionRasterFallback = 16;
inline constexpr std::uint32_t kRegionAttribution = 17;
inline constexpr std::uint32_t kRegionSystem = 18;
inline constexpr std::uint32_t kRegionLineRegs = 19;
inline constexpr std::uint32_t kRegionLineCram = 20;
inline constexpr std::uint32_t kRegionLineCells = 21;
inline constexpr std::uint32_t kRegionRasterJournal = 22;
inline constexpr std::uint32_t kRegionPalette = 24;
inline constexpr std::uint32_t kRegionSpriteOutcome = 25;
inline constexpr std::uint32_t kRegionZ80Ram = 26;
/// The work RAM has no AYTHER region id: it is the legacy system memory.
inline constexpr std::uint32_t kRegionWorkRam = 0x1000;

/// Size in bytes of a memory region, 0 if the id is not one of them.
constexpr std::size_t memory_region_size(std::uint32_t id) noexcept {
  switch (id) {
  case kRegionVram:
    return 65536;
  case kRegionCram:
    return 128;
  case kRegionVdpRegs:
    return 32;
  case kRegionVsram:
    return 128;
  case kRegionParsedSprites:
    return 1280;
  case kRegionParsedSpriteCount:
    return 1;
  case kRegionRasterFallback:
    return 4;
  case kRegionSystem:
    return 44;
  case kRegionRasterJournal:
    return 2064;
  case kRegionSpriteOutcome:
    return 80;
  case kRegionPalette:
    return 512;
  case kRegionZ80Ram:
    return 8192;
  case kRegionWorkRam:
    return 65536;
  default:
    return 0;
  }
}

/// Interlace mode 2 in force (register 12 bits 1 and 2).
inline bool double_resolution(const System &system) noexcept {
  return (system.vdp().reg(12) & 0x06U) == 0x06U;
}

/// gpgx leaves LINE_REGS and LINE_CRAM without entries in double resolution
/// and with the 2-cell vertical scroll (register 11 bit 2) (measured,
/// modes_sweep 3, 4, 15, 16, 23 to 27). Taken from the registers in force when
/// the region is read; a mode switched on and off within one frame is not
/// measured.
inline bool line_state_unavailable(const System &system) noexcept {
  return double_resolution(system) || (system.vdp().reg(11) & 0x04U) != 0;
}

/// True for the per-line regions LINE_REGS, LINE_CRAM and LINE_CELLS.
constexpr bool line_region(std::uint32_t id) noexcept {
  return id == kRegionLineRegs || id == kRegionLineCram ||
         id == kRegionLineCells;
}

/// Byte size of one entry of a per-line region.
constexpr std::uint32_t line_entry_size(std::uint32_t id) noexcept {
  return id == kRegionLineRegs ? 32U : id == kRegionLineCram ? 128U : 264U;
}

/// Regions whose size varies with the frame (ATTRIBUTION and the per-line
/// ones), served in Mode 5 only. Their content may legitimately be empty.
inline bool variable_region(const System &system, std::uint32_t id) noexcept {
  return (line_region(id) || id == kRegionAttribution) &&
         !system.vdp().mode4_active();
}

/// Current byte size of ATTRIBUTION, LINE_REGS, LINE_CRAM or LINE_CELLS (what
/// memory_region returns for them), or 0 for any other region or in Mode 4
/// (not served there).
inline std::uint32_t line_region_size(const System &system,
                                      std::uint32_t id) noexcept {
  if (id == kRegionAttribution) {
    // One byte per pixel of the drawn frame (57344 in H32, 71680 in H40).
    if (system.vdp().mode4_active() || !system.renderer().attribution_drawn())
      return 0;
    const auto size = system.renderer().size();
    return size.width * size.height;
  }
  if (!line_region(id) || system.vdp().mode4_active())
    return 0;
  if (line_state_unavailable(system))
    return 24U;
  const std::uint32_t lines = system.renderer().size().height;
  if (id != kRegionLineCram)
    return 24U + lines * line_entry_size(id);
  return 24U + (system.vdp().raster().cram_changed ? lines : 1U) * 128U;
}

/// Whole content of a memory region as the contract lays it out. Empty for an
/// id that is not a memory region.
inline std::vector<std::uint8_t> memory_region(const System &system,
                                               std::uint32_t id) {
  std::vector<std::uint8_t> out(memory_region_size(id), 0);
  const auto word_le = [&](std::size_t at, std::uint16_t value) {
    out[at] = static_cast<std::uint8_t>(value);
    out[at + 1] = static_cast<std::uint8_t>(value >> 8);
  };
  const auto u32_le = [&](std::size_t at, std::uint32_t value) {
    word_le(at, static_cast<std::uint16_t>(value));
    word_le(at + 2, static_cast<std::uint16_t>(value >> 16));
  };
  if (id == kRegionAttribution) {
    if (system.vdp().mode4_active())
      return {}; // Mode 4 is not served (ABI: Mode 5 merge only)
    const auto &attribution = system.renderer().attribution();
    out.assign(line_region_size(system, id), 0); // 0 bytes if nothing drawn
    std::copy_n(attribution.begin(), std::min(attribution.size(), out.size()),
                out.begin());
    return out;
  }
  if (line_region(id)) {
    // tessera_obs_line_header_v1 then one entry per visible line. LINE_CRAM
    // shrinks to one entry, with flags 1, when the CRAM did not change on a
    // visible line; LINE_REGS and LINE_CELLS carry the same flag (measured on
    // gpgx, raster_bands 0 to 33). Sizes vary: 7192 / 28696 bytes at 224
    // lines (LINE_CELLS 59160).
    if (system.vdp().mode4_active())
      return {};
    const auto &renderer = system.renderer();
    const std::uint32_t lines = renderer.size().height;
    const bool regs = id == kRegionLineRegs;
    const bool cells = id == kRegionLineCells;
    const std::uint32_t entry = line_entry_size(id);
    if (line_state_unavailable(system)) {
      // The header alone, count 0 and flags 0 (measured on gpgx, modes_sweep
      // 3, 4, 15, 16 and 23 to 27, sprite_limits 220 to 222).
      out.assign(24U, 0);
      u32_le(0, 24);
      u32_le(4, entry);
      u32_le(16, static_cast<std::uint32_t>(system.frame()));
      u32_le(20, static_cast<std::uint32_t>(system.frame() >> 32));
      return out;
    }
    const bool cram_steady = !system.vdp().raster().cram_changed;
    const std::uint32_t count = regs || cells || !cram_steady ? lines : 1U;
    out.assign(24U + std::size_t{count} * entry, 0);
    u32_le(0, 24);
    u32_le(4, entry);
    u32_le(8, count);
    u32_le(12, cram_steady ? 1U : 0U);
    u32_le(16, static_cast<std::uint32_t>(system.frame()));
    u32_le(20, static_cast<std::uint32_t>(system.frame() >> 32));
    for (std::uint32_t y = 0; y < count; ++y) {
      std::uint8_t *at = out.data() + 24U + std::size_t{y} * entry;
      if (cells) {
        std::copy_n(renderer.line_cells()[y].begin(), 264, at);
      } else if (regs) {
        std::copy_n(renderer.line_regs()[y].begin(), 32, at);
      } else {
        if (cram_steady) {
          // One entry: the CRAM of the whole frame.
          const auto &live = system.vdp().cram();
          for (unsigned i = 0; i < 64U; ++i) {
            const std::uint16_t c = live[i];
            const auto packed = static_cast<std::uint16_t>(
                ((c >> 9 & 7U) << 6) | ((c >> 5 & 7U) << 3) | (c >> 1 & 7U));
            at[2 * i] = static_cast<std::uint8_t>(packed);
            at[2 * i + 1] = static_cast<std::uint8_t>(packed >> 8);
          }
        } else {
          const auto &cram = renderer.line_cram()[y];
          for (unsigned i = 0; i < 64U; ++i) {
            at[2 * i] = static_cast<std::uint8_t>(cram[i]);
            at[2 * i + 1] = static_cast<std::uint8_t>(cram[i] >> 8);
          }
        }
      }
    }
    return out;
  }
  switch (id) {
  case kRegionRasterFallback:
    // 0x20 also stands for a double resolution frame (modes_sweep 23 to 27).
    u32_le(0, system.vdp().raster().fallback |
                  (double_resolution(system) ? 0x20U : 0U));
    break;
  case kRegionRasterJournal: {
    // tessera_obs_journal_v1: version, size, count, dropped, 256 events.
    const auto &raster = system.vdp().raster();
    u32_le(0, 1);
    u32_le(4, 2064);
    u32_le(8, raster.count);
    u32_le(12, raster.dropped);
    for (std::uint32_t i = 0; i < raster.count; ++i) {
      const auto &e = raster.events[i];
      word_le(16 + 8 * i, e.v_counter);
      word_le(18 + 8 * i, e.reason);
      word_le(20 + 8 * i, e.address);
      word_le(22 + 8 * i, e.data);
    }
    break;
  }
  case kRegionVram: {
    const auto &vram = system.vdp().vram();
    // Mode 4 keeps its data in the layout of the reference buffer (the low
    // byte at the even address, see Vdp::write_data); Mode 5 is word-swapped.
    const std::size_t flip = system.vdp().mode4_active() ? 0U : 1U;
    for (std::size_t i = 0; i < out.size(); ++i)
      out[i ^ flip] = vram[i];
    break;
  }
  case kRegionCram: {
    const auto &cram = system.vdp().cram();
    // The reference core keeps each colour as nine bits BBBGGGRRR; the VDP
    // word is 0BBB0GGG0RRR.
    for (std::size_t i = 0; i < cram.size(); ++i)
      word_le(2 * i, static_cast<std::uint16_t>(((cram[i] >> 9 & 7U) << 6) |
                                                ((cram[i] >> 5 & 7U) << 3) |
                                                (cram[i] >> 1 & 7U)));
    break;
  }
  case kRegionVdpRegs:
    for (unsigned i = 0; i < 32; ++i)
      out[i] = system.vdp().reg(i);
    break;
  case kRegionVsram: {
    const auto &vsram = system.vdp().vsram();
    for (std::size_t i = 0; i < vsram.size(); ++i)
      word_le(2 * i, vsram[i]);
    break;
  }
  case kRegionZ80Ram: {
    const auto ram = system.z80_ram();
    std::copy(ram.begin(), ram.end(), out.begin());
    break;
  }
  case kRegionParsedSprites: {
    // tessera_obs_sprite_v1 x 128, little endian, 10 bytes each
    const auto &capture = system.renderer().sprite_capture();
    // all the slots: past the count they hold what earlier frames left
    for (unsigned i = 0; i < capture.sprites.size(); ++i) {
      const auto &s = capture.sprites[i];
      const std::size_t at = std::size_t{i} * 10U;
      word_le(at, s.yr);
      word_le(at + 2, s.xr);
      word_le(at + 4, s.attr);
      out[at + 6] = s.w;
      out[at + 7] = s.h;
      out[at + 8] = s.sat_idx;
      out[at + 9] = s.chain_pos;
    }
    break;
  }
  case kRegionParsedSpriteCount:
    out[0] =
        static_cast<std::uint8_t>(system.renderer().sprite_capture().count);
    break;
  case kRegionSpriteOutcome: {
    const auto &outcome = system.renderer().sprite_capture().outcome;
    std::copy(outcome.begin(), outcome.end(), out.begin());
    break;
  }
  case kRegionPalette: {
    // 256 resolved colours RGB565, little endian
    const auto table = palette_565(system.vdp());
    for (std::size_t i = 0; i < table.size(); ++i)
      word_le(2 * i, table[i]);
    break;
  }
  case kRegionSystem: {
    // tessera_obs_system_v1, little endian, 44 bytes.
    const auto &vdp = system.vdp();
    const bool pal = system.config().standard == Standard::pal;
    const bool mode4 = vdp.mode4_active();
    const unsigned r12 = vdp.reg(12);
    // Geometry of the frame drawn (fixed at line 0), not of the registers in
    // force; any size change during the frame sets GEOMETRY_PENDING
    // (measured on gpgx, raster_bands 7 and 15 to 18).
    const FrameSize frame = system.renderer().size();
    const bool h40 = !mode4 && frame.width == 320U;
    const auto u32 = [&](std::size_t at, std::uint32_t value) {
      word_le(at, static_cast<std::uint16_t>(value));
      word_le(at + 2, static_cast<std::uint16_t>(value >> 16));
    };
    u32(0, 44);              // struct_size
    u32(4, 1);               // layout_version
    out[8] = 0x80;           // system_hw: Mega Drive
    out[9] = pal ? 1 : 0;    // region_pal
    out[10] = mode4 ? 0 : 5; // vdp_mode
    out[11] = mode4 ? 0 : (r12 & 0x02U) != 0 ? ((r12 & 0x04U) != 0 ? 2 : 1) : 0;
    out[12] = h40 ? 1 : 0;                                 // h40
    out[13] = !mode4 && (r12 & 0x08U) != 0 ? 1 : 0;        // shadow_highlight
    word_le(14, pal ? 313 : 262);                          // lines_per_frame
    word_le(16, 0);                                        // viewport_x
    word_le(18, 0);                                        // viewport_y
    word_le(20, static_cast<std::uint16_t>(frame.width));  // viewport_w
    word_le(22, static_cast<std::uint16_t>(frame.height)); // viewport_h
    u32(24, pal ? 7600489U : 7670453U);                    // cpu_clock
    u32(28, pal ? 53203424U : 53693175U);                  // master_clock
    out[32] = 0;                                           // fm_core
    out[33] = 1;                                           // psg_present
    out[34] = 0;                                           // pcm_present
    out[35] = vdp.geometry_changed() ? 1 : 0; // flags: GEOMETRY_PENDING
    u32(36, system.rom_crc32());
    u32(40, system.rom_bytes());
    break;
  }
  case kRegionWorkRam: {
    const auto ram = system.work_ram();
    for (std::size_t i = 0; i < out.size(); ++i)
      out[i ^ 1U] = ram[i];
    break;
  }
  default:
    out.clear();
  }
  return out;
}

/// Control write on a memory region (RF-3.1): the poke of the work RAM and of
/// the Z80 RAM. `offset` counts bytes of the region as `memory_region` lays it
/// out, so on the word-swapped work RAM the byte `offset` is the logical byte
/// `offset ^ 1`. False if the region takes no poke or the range is out of it.
inline bool poke_memory_region(System &system, std::uint32_t id,
                               std::size_t offset,
                               std::span<const std::uint8_t> bytes) {
  const std::size_t size = memory_region_size(id);
  if (bytes.empty() || offset >= size || bytes.size() > size - offset)
    return false;
  switch (id) {
  case kRegionZ80Ram:
    return system.poke_z80_ram(offset, bytes);
  case kRegionWorkRam:
    for (std::size_t i = 0; i < bytes.size(); ++i) {
      const std::uint8_t one = bytes[i];
      if (!system.poke_work_ram((offset + i) ^ 1U, std::span(&one, 1)))
        return false;
    }
    return true;
  default:
    return false; // VRAM, CRAM, VSRAM and the registers are read only
  }
}

} // namespace tessera::md
