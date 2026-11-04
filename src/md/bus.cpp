#include "md/bus.hpp"

namespace tessera::md {
namespace {
constexpr std::uint32_t kMask = 0xFFFFFF;

std::uint32_t device_offset(Target t, std::uint32_t a) noexcept {
  switch (t) {
  case Target::z80_area:
    return a - 0xA00000;
  case Target::io:
  case Target::z80_control:
    return a - 0xA10000;
  case Target::vdp:
    return a & 0x1F;
  default:
    return a;
  }
}
} // namespace

Target classify(std::uint32_t address) noexcept {
  const std::uint32_t a = address & kMask;
  if (a < 0x400000)
    return Target::rom;
  if (a >= 0xE00000)
    return Target::work_ram;
  if (a >= 0xC00000 && a < 0xE00000)
    return Target::vdp;
  if (a >= 0xA00000 && a < 0xA10000)
    return Target::z80_area;
  if (a >= 0xA10000 && a < 0xA11000)
    return Target::io;
  if (a >= 0xA11000 && a < 0xA12000)
    return Target::z80_control;
  return Target::unmapped;
}

Bus::Bus(std::vector<std::uint8_t> rom, BusDevices &devices)
    : rom_(std::move(rom)), devices_(devices) {}

std::uint8_t Bus::read8(std::uint32_t address) {
  const std::uint32_t a = address & kMask;
  switch (const Target t = classify(a)) {
  case Target::rom:
    return a < rom_.size() ? rom_[a] : kUnmappedByte;
  case Target::work_ram:
    return ram_[a & 0xFFFF];
  case Target::unmapped:
    return kUnmappedByte;
  default:
    return static_cast<std::uint8_t>(
        devices_.read(t, device_offset(t, a), false));
  }
}

std::uint16_t Bus::read16(std::uint32_t address) {
  const std::uint32_t a = address & kMask & ~1U;
  switch (const Target t = classify(a)) {
  case Target::rom:
    return a + 1 < rom_.size()
               ? static_cast<std::uint16_t>((rom_[a] << 8) | rom_[a + 1])
               : kUnmappedWord;
  case Target::work_ram:
    return static_cast<std::uint16_t>((ram_[a & 0xFFFF] << 8) |
                                      ram_[(a + 1) & 0xFFFF]);
  case Target::unmapped:
    return kUnmappedWord;
  default:
    return devices_.read(t, device_offset(t, a), true);
  }
}

std::uint32_t Bus::read32(std::uint32_t address) {
  const std::uint32_t high = read16(address);
  return (high << 16) | read16(address + 2);
}

void Bus::write8(std::uint32_t address, std::uint8_t value) {
  const std::uint32_t a = address & kMask;
  switch (const Target t = classify(a)) {
  case Target::rom:
  case Target::unmapped:
    return;
  case Target::work_ram:
    ram_[a & 0xFFFF] = value;
    return;
  default:
    devices_.write(t, device_offset(t, a), value, false);
    return;
  }
}

void Bus::write16(std::uint32_t address, std::uint16_t value) {
  const std::uint32_t a = address & kMask & ~1U;
  switch (const Target t = classify(a)) {
  case Target::rom:
  case Target::unmapped:
    return;
  case Target::work_ram:
    ram_[a & 0xFFFF] = static_cast<std::uint8_t>(value >> 8);
    ram_[(a + 1) & 0xFFFF] = static_cast<std::uint8_t>(value);
    return;
  default:
    devices_.write(t, device_offset(t, a), value, true);
    return;
  }
}

void Bus::write32(std::uint32_t address, std::uint32_t value) {
  write16(address, static_cast<std::uint16_t>(value >> 16));
  write16(address + 2, static_cast<std::uint16_t>(value));
}

} // namespace tessera::md
