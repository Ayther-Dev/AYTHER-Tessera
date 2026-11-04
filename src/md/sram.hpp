// Cartridge SRAM and its .srm file (spec 008, TP-101.6).
// The .srm is byte-compatible with gpgx: the raw image of the address range
// declared in the header, from the even address at or below `start` up to the
// odd address at or above `end`. On an 8 bit bus the other parity holds 0xFF.
#pragma once
#include "md/cart.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace tessera::md {

class Sram {
public:
  explicit Sram(const SramInfo &info);

  std::size_t image_size() const noexcept { return image_.size(); }
  /// True when `address` falls inside the image.
  bool contains(std::uint32_t address) const noexcept {
    return address >= base_ && address - base_ < image_.size();
  }
  bool dirty() const noexcept { return dirty_; }

  /// Bus access; addresses outside the range read 0xFF and are not written.
  std::uint8_t read8(std::uint32_t address) const noexcept;
  void write8(std::uint32_t address, std::uint8_t value) noexcept;

  /// Loads a .srm. A file larger than the image is rejected; a shorter one
  /// fills the start and leaves the rest erased (as gpgx does).
  bool load_srm(std::span<const std::uint8_t> file);
  std::vector<std::uint8_t> to_srm() const { return image_; }

  /// Writes `path` through `path + ".tmp"` and a rename, so a failure never
  /// leaves a half-written file. Clears the dirty flag on success.
  bool save_atomic(const std::string &path);

private:
  bool writable(std::uint32_t address) const noexcept;

  SramInfo info_;
  std::uint32_t base_ = 0; ///< even address of the first image byte
  std::vector<std::uint8_t> image_;
  bool dirty_ = false;
};

} // namespace tessera::md
