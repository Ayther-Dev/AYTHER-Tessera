// Cartridge loader of the native Mega Drive core (spec 008, TP-101.3).
// Accepts plain .bin/.md, a 512 byte copier header and interleaved .smd, and
// reads the region and SRAM fields of the ROM header.
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

inline constexpr std::size_t kMaxRomSize = 16U * 1024U * 1024U; // 16 MiB

inline constexpr std::uint8_t kRegionJapan = 1;
inline constexpr std::uint8_t kRegionUsa = 2;
inline constexpr std::uint8_t kRegionEurope = 4;

enum class CartFormat : std::uint8_t { plain, copier_header, smd };
enum class SramBus : std::uint8_t { both, odd, even };

struct SramInfo {
  bool present = false;
  std::uint32_t start = 0;
  std::uint32_t end = 0;
  SramBus bus = SramBus::both;
  std::uint32_t size_bytes = 0; ///< bytes of storage, not of address range
};

struct Cart {
  CartFormat format = CartFormat::plain;
  std::vector<std::uint8_t> rom; ///< plain, de-interleaved, header-less
  std::uint8_t regions = 0;      ///< bit mask of kRegion*; 0 = not declared
  SramInfo sram;
};

/// Throws tessera::Failure{TESSERA_INVALID_CONTENT} for content that is not a
/// Mega Drive ROM or exceeds 16 MiB.
Cart load_cart(std::span<const std::uint8_t> content);

/// SRAM declared by the header of an already plain ROM image ("RA" at $1B0);
/// `present` is false when there is none or the image is too short.
SramInfo read_sram_header(std::span<const std::uint8_t> rom);

} // namespace tessera::md
