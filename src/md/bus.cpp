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
  case Target::mapper:
    return a - 0xA130F0;
  case Target::tmss:
    return a - 0xA14000;
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
  if (a >= 0xA130F0 && a <= 0xA130FF)
    return Target::mapper;
  if (a >= 0xA14000 && a < 0xA14004)
    return Target::tmss;
  return Target::unmapped;
}

Bus::Bus(std::vector<std::uint8_t> rom, BusDevices &devices, Mapper mapper)
    : rom_(std::move(rom)), devices_(devices), mapper_(mapper) {}

// Offset inside the ROM image of a cartridge address. With SSF2, slot 0
// (first 512 KiB) is fixed and slots 1-7 follow the bank registers; a bank
// past the end of the image wraps over it.
std::uint32_t Bus::rom_offset(std::uint32_t a) const noexcept {
  if (mapper_ != Mapper::ssf2 || a < 0x80000 || rom_.empty())
    return a;
  const std::uint64_t offset =
      std::uint64_t{banks_[a >> 19]} * 0x80000U + (a & 0x7FFFFU);
  return static_cast<std::uint32_t>(offset % rom_.size());
}

void Bus::write_mapper(std::uint32_t a, std::uint8_t value) noexcept {
  if (mapper_ != Mapper::ssf2)
    return;
  const std::uint32_t reg = a & 0xF; // registers live on odd bytes
  if (reg == 1)
    sram_mapped_ = (value & 1U) != 0;
  else if ((reg & 1U) != 0 && reg >= 3)
    banks_[(reg - 1) / 2] = value & 0x3F;
}

std::uint8_t Bus::read8(std::uint32_t address) {
  const std::uint32_t a = address & kMask;
  switch (const Target t = classify(a)) {
  case Target::rom: {
    const std::uint32_t o = rom_offset(a);
    return o < rom_.size() ? rom_[o] : kUnmappedByte;
  }
  case Target::work_ram:
    return ram_[a & 0xFFFF];
  case Target::unmapped:
  case Target::mapper:
    return kUnmappedByte;
  default:
    return static_cast<std::uint8_t>(
        devices_.read(t, device_offset(t, a), false));
  }
}

std::uint16_t Bus::read16(std::uint32_t address) {
  const std::uint32_t a = address & kMask & ~1U;
  switch (const Target t = classify(a)) {
  case Target::rom: {
    const std::uint32_t o = rom_offset(a);
    return o + 1 < rom_.size()
               ? static_cast<std::uint16_t>((rom_[o] << 8) | rom_[o + 1])
               : kUnmappedWord;
  }
  case Target::work_ram:
    return static_cast<std::uint16_t>((ram_[a & 0xFFFF] << 8) |
                                      ram_[(a + 1) & 0xFFFF]);
  case Target::unmapped:
  case Target::mapper:
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
  case Target::mapper:
    write_mapper(a, value);
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
  case Target::mapper:
    write_mapper(a | 1U, static_cast<std::uint8_t>(value)); // low byte, odd reg
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
