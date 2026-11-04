#include "md/sram.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace tessera::md {

Sram::Sram(const SramInfo &info) : info_(info) {
  if (!info.present)
    return;
  base_ = info.start & ~1U;
  image_.assign((info.end | 1U) - base_ + 1U, 0xFF);
}

bool Sram::writable(std::uint32_t address) const noexcept {
  if (!info_.present || address < base_ || address - base_ >= image_.size())
    return false;
  switch (info_.bus) {
  case SramBus::odd:
    return (address & 1U) != 0;
  case SramBus::even:
    return (address & 1U) == 0;
  case SramBus::both:
    return true;
  }
  return false;
}

std::uint8_t Sram::read8(std::uint32_t address) const noexcept {
  if (address < base_ || address - base_ >= image_.size())
    return 0xFF;
  return image_[address - base_];
}

void Sram::write8(std::uint32_t address, std::uint8_t value) noexcept {
  if (!writable(address))
    return;
  image_[address - base_] = value;
  dirty_ = true;
}

bool Sram::load_srm(std::span<const std::uint8_t> file) {
  if (file.size() > image_.size())
    return false;
  std::fill(image_.begin(), image_.end(), std::uint8_t{0xFF});
  std::copy(file.begin(), file.end(), image_.begin());
  dirty_ = false;
  return true;
}

bool Sram::save_atomic(const std::string &path) {
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    out.write(reinterpret_cast<const char *>(image_.data()),
              static_cast<std::streamsize>(image_.size()));
    out.flush();
    if (!out) {
      out.close();
      std::error_code ignored;
      std::filesystem::remove(tmp, ignored);
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  dirty_ = false;
  return true;
}

} // namespace tessera::md
