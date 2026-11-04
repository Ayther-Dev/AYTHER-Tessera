#include "md/tmss.hpp"

namespace tessera::md {
namespace {
bool sega_at(std::span<const std::uint8_t> rom, std::size_t at) noexcept {
  return rom.size() >= at + 4 && rom[at] == 'S' && rom[at + 1] == 'E' &&
         rom[at + 2] == 'G' && rom[at + 3] == 'A';
}
} // namespace

bool header_has_sega(std::span<const std::uint8_t> rom) noexcept {
  return sega_at(rom, 0x100) || sega_at(rom, 0x101);
}

bool can_boot(const MachineConfig &config,
              std::span<const std::uint8_t> rom) noexcept {
  return !config.tmss || header_has_sega(rom);
}

void Tmss::write(std::uint32_t offset, std::uint16_t value,
                 bool word) noexcept {
  if (offset > 3)
    return;
  if (word) {
    reg_[offset & 2U] = static_cast<std::uint8_t>(value >> 8);
    reg_[(offset & 2U) + 1U] = static_cast<std::uint8_t>(value);
  } else {
    reg_[offset] = static_cast<std::uint8_t>(value);
  }
  unlocked_ =
      reg_[0] == 'S' && reg_[1] == 'E' && reg_[2] == 'G' && reg_[3] == 'A';
}

void Tmss::reset() noexcept {
  unlocked_ = false;
  reg_ = {};
}

} // namespace tessera::md
