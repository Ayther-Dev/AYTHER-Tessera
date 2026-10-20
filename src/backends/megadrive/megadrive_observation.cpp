#include "backends/megadrive/megadrive_backend.hpp"

#include "domain/result.hpp"
#include "domain/wire.hpp"

namespace tessera {

RegionData MegaDriveBackend::temporal_region() const {
  // Versioned raster evidence: geometry, then each rendered line's palette
  // and colour indices in scanline order. This preserves mid-frame CRAM
  // changes. It does not claim tile or sprite resource provenance; Engine
  // must use the complete original frame when that provenance is required.
  Bytes data;
  wire::scalar(data, 1, TESSERA_ENCODING_U32, 1);
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
  return {5, TESSERA_ENCODING_BYTES, 0, 1,
          static_cast<std::uint32_t>(data.size()), 0,
          temporal_complete_ ? 1U : 0x80000000U, std::move(data)};
}

} // namespace tessera
