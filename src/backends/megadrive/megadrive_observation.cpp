#include "backends/megadrive/megadrive_backend.hpp"

#include "domain/result.hpp"
#include "domain/wire.hpp"

#include <algorithm>

namespace tessera {

std::array<std::uint8_t, 8 * 8 * 4>
MegaDriveBackend::tile_rgba(std::uint16_t word) const {
  std::array<std::uint8_t, 8 * 8 * 4> pixels{};
  const auto &vram = machine_->vdp.state.vram;
  const auto tile = std::uint32_t(word & 0x7FF);
  const auto palette = std::uint32_t((word >> 13) & 3);
  for (std::uint32_t y = 0; y < 8; ++y)
    for (std::uint32_t x = 0; x < 8; ++x) {
      const auto pattern = vram[tile * 32 + y * 4 + x / 2];
      const auto index = (x & 1) ? (pattern & 15) : (pattern >> 4);
      const auto colour = palette_[palette * 16 + index];
      const auto offset = (y * 8 + x) * 4;
      pixels[offset] = static_cast<std::uint8_t>((colour & 15) * 17);
      pixels[offset + 1] =
          static_cast<std::uint8_t>(((colour >> 4) & 15) * 17);
      pixels[offset + 2] =
          static_cast<std::uint8_t>(((colour >> 8) & 15) * 17);
      pixels[offset + 3] = index == 0 ? 0 : 255;
    }
  return pixels;
}

void MegaDriveBackend::capture_provenance(
    std::uint32_t line, const VDP_ProvenancePixel *sources,
    std::uint32_t left, std::uint32_t right, std::uint32_t width,
    std::uint32_t height) {
  if (!temporal_requested_)
    return;
  if (width != 320 || height != 224 || line >= height || left > right ||
      right > width) {
    temporal_source_complete_ = false;
    return;
  }
  const auto &state = machine_->vdp.state;
  if (!state.display_enabled || state.extended_vram_enabled ||
      state.double_resolution_enabled || state.shadow_highlight_enabled ||
      state.debug.hide_layers || state.debug.forced_layer != 0 ||
      state.plane_a_tile_index_rebase || state.hscroll_mask != 0 ||
      state.vsram_cache[0] != 0 || state.vsram_cache[1] != 0 ||
      state.window.horizontal_boundary != 0 ||
      state.window.vertical_boundary != 0) {
    temporal_source_complete_ = false;
    return;
  }
  if (temporal_sources_.empty()) {
    temporal_sources_.resize(std::size_t(width) * height);
    temporal_cells_.resize(std::size_t(width / 8) * (height / 8));
  }
  for (std::uint32_t x = left; x < right; ++x) {
    const auto source = sources[x];
    temporal_sources_[std::size_t(line) * width + x] = source;
    if (source.source != 1)
      continue;
    if (source.source_x != x % 8 || source.source_y != line % 8 ||
        (source.tile_word & 0x1800) != 0) {
      temporal_source_complete_ = false;
      continue;
    }
    auto &cell = temporal_cells_[std::size_t(line / 8) * (width / 8) + x / 8];
    const auto row_bit = static_cast<std::uint8_t>(1U << (line % 8));
    if (!cell.observed || (cell.rows_seen & row_bit) == 0) {
      const auto image = tile_rgba(source.tile_word);
      if (cell.observed &&
          (cell.tile_word != source.tile_word || cell.rgba != image))
        temporal_source_complete_ = false;
      if (!cell.observed) {
        cell.observed = true;
        cell.tile_word = source.tile_word;
        cell.rgba = image;
      }
    } else if (cell.tile_word != source.tile_word) {
      temporal_source_complete_ = false;
    }
    cell.visible |= std::uint64_t{1} << ((line % 8) * 8 + x % 8);
    cell.rows_seen |= row_bit;
  }
}

RegionData MegaDriveBackend::temporal_region() const {
  // Version 2 adds winning source metadata and tile images sampled during
  // rendering. No end-of-frame VRAM inference is used for replacements.
  Bytes data;
  wire::scalar(data, 1, TESSERA_ENCODING_U32, 2);
  wire::scalar(data, 2, TESSERA_ENCODING_U32, width_);
  wire::scalar(data, 3, TESSERA_ENCODING_U32, height_);
  wire::scalar(data, 4, TESSERA_ENCODING_U32, temporal_complete_);
  if (temporal_complete_) {
    Bytes raster;
    raster.reserve(std::size_t(height_) * (width_ + 512));
    for (std::uint32_t line = 0; line < height_; ++line) {
      for (const auto colour : temporal_palettes_[line]) {
        raster.push_back(static_cast<std::uint8_t>(colour));
        raster.push_back(static_cast<std::uint8_t>(colour >> 8));
      }
      const auto start = temporal_indices_.begin() + std::size_t(line) * 1024;
      raster.insert(raster.end(), start, start + width_);
    }
    wire::field(data, 5, TESSERA_ENCODING_BYTES, raster);
  }
  wire::scalar(data, 6, TESSERA_ENCODING_U32, temporal_source_complete_);
  if (temporal_complete_ && temporal_source_complete_) {
    Bytes cells;
    auto append = [&](std::uint64_t value, unsigned count) {
      for (unsigned index = 0; index < count; ++index)
        cells.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
    };
    const auto count = static_cast<std::uint32_t>(
        std::count_if(temporal_cells_.begin(), temporal_cells_.end(),
                      [](const TemporalCell &cell) { return cell.observed; }));
    append(count, 4);
    for (std::size_t index = 0; index < temporal_cells_.size(); ++index) {
      const auto &cell = temporal_cells_[index];
      if (!cell.observed)
        continue;
      append((index % (width_ / 8)) * 8, 2);
      append((index / (width_ / 8)) * 8, 2);
      append(cell.tile_word, 2);
      append(0, 2);
      append(cell.visible, 8);
      cells.insert(cells.end(), cell.rgba.begin(), cell.rgba.end());
    }
    wire::field(data, 7, TESSERA_ENCODING_BYTES, cells);
    Bytes provenance;
    provenance.reserve(temporal_sources_.size() * 8);
    for (const auto &source : temporal_sources_) {
      provenance.push_back(static_cast<std::uint8_t>(source.tile_index));
      provenance.push_back(static_cast<std::uint8_t>(source.tile_index >> 8));
      provenance.push_back(static_cast<std::uint8_t>(source.tile_word));
      provenance.push_back(static_cast<std::uint8_t>(source.tile_word >> 8));
      provenance.push_back(source.source);
      provenance.push_back(source.sat_slot);
      provenance.push_back(source.source_x);
      provenance.push_back(source.source_y);
    }
    wire::field(data, 8, TESSERA_ENCODING_BYTES, provenance);
  }
  return {5, TESSERA_ENCODING_BYTES, 0, 1,
          static_cast<std::uint32_t>(data.size()), 0,
          temporal_complete_ && temporal_source_complete_
              ? 3U : 0x80000000U,
          std::move(data)};
}

} // namespace tessera
