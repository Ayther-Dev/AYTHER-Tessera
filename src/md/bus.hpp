// 68000 bus of the native Mega Drive core (spec 008, TP-101.4).
// Routes the 24 bit address space to the cartridge ROM, the work RAM and the
// devices (Z80 area, I/O, Z80 control, VDP). Multi-byte accesses are big
// endian; a long access is two word accesses, high word first.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tessera::md {

// Value read from an address nothing answers at. Provisional until the plane
// «memoria» of the differential bench compares it with gpgx (TP-005, G1).
inline constexpr std::uint8_t kUnmappedByte = 0xFF;
inline constexpr std::uint16_t kUnmappedWord = 0xFFFF;

enum class Target : std::uint8_t {
  rom,         ///< 000000-3FFFFF
  z80_area,    ///< A00000-A0FFFF (offset from A00000)
  io,          ///< A10000-A10FFF (offset from A10000)
  z80_control, ///< A11000-A11FFF (offset from A10000: bus request, reset)
  vdp,         ///< C00000-DFFFFF (offset = address & 0x1F)
  work_ram,    ///< E00000-FFFFFF, 64 KiB mirrored
  unmapped
};

Target classify(std::uint32_t address) noexcept;

// What lives behind the bus. `word` tells a 16 bit access from an 8 bit one;
// for bytes the value is in the low 8 bits.
class BusDevices {
public:
  virtual ~BusDevices() = default;
  virtual std::uint16_t read(Target target, std::uint32_t offset,
                             bool word) = 0;
  virtual void write(Target target, std::uint32_t offset, std::uint16_t value,
                     bool word) = 0;
};

class Bus {
public:
  Bus(std::vector<std::uint8_t> rom, BusDevices &devices);

  std::uint8_t read8(std::uint32_t address);
  std::uint16_t read16(std::uint32_t address);
  std::uint32_t read32(std::uint32_t address);
  void write8(std::uint32_t address, std::uint8_t value);
  void write16(std::uint32_t address, std::uint16_t value);
  void write32(std::uint32_t address, std::uint32_t value);

  std::span<const std::uint8_t> work_ram() const noexcept { return ram_; }
  std::span<std::uint8_t> work_ram() noexcept { return ram_; }

private:
  std::vector<std::uint8_t> rom_;
  std::array<std::uint8_t, 0x10000> ram_{};
  BusDevices &devices_;
};

} // namespace tessera::md
