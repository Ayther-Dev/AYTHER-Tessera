#include "md/special_cart.hpp"
#include "md/system.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace tessera::md {

Eeprom::Eeprom(const EepromSpec &spec) : spec_(spec) {
  if (spec_.protocol != EepromProtocol::none && spec_.size_bytes > 0) {
    image_.assign(spec_.size_bytes, 0xFF);
    prev_cs_ = (spec_.protocol == EepromProtocol::spi);
  }
}

bool Eeprom::contains_addr(std::uint32_t address) const noexcept {
  if (!present())
    return false;
  const std::uint32_t a = address & 0xFFFFFFU;
  return a == spec_.pins.sda_in_addr || a == spec_.pins.sda_out_addr ||
         a == spec_.pins.scl_addr || a == spec_.pins.cs_addr;
}

std::uint8_t Eeprom::read8(std::uint32_t address) const noexcept {
  if (!present())
    return 0xFF;
  const std::uint32_t a = address & 0xFFFFFFU;
  if (a != spec_.pins.sda_out_addr)
    return 0xFF;
  return out_bit_ ? static_cast<std::uint8_t>(1U << spec_.pins.sda_out_bit) : 0;
}

void Eeprom::write8(std::uint32_t address, std::uint8_t value) noexcept {
  if (!present())
    return;
  const std::uint32_t a = address & 0xFFFFFFU;
  bool scl = prev_scl_;
  bool sda = prev_sda_;
  bool cs = prev_cs_;
  if (a == spec_.pins.scl_addr)
    scl = (value & (1U << spec_.pins.scl_bit)) != 0;
  if (a == spec_.pins.sda_in_addr)
    sda = (value & (1U << spec_.pins.sda_in_bit)) != 0;
  if (a == spec_.pins.cs_addr)
    cs = (value & (1U << spec_.pins.cs_bit)) != 0;

  switch (spec_.protocol) {
  case EepromProtocol::i2c:
    drive_i2c(scl, sda);
    break;
  case EepromProtocol::spi:
    drive_spi(!cs, scl, sda);
    break;
  case EepromProtocol::microwire:
    drive_microwire(cs, scl, sda);
    break;
  case EepromProtocol::none:
    break;
  }
}

void Eeprom::i2c_start() noexcept {
  i2c_bit_ = 0;
  i2c_shift_ = 0;
  i2c_state_ = spec_.i2c_7bit_word_addr ? I2cState::get_word_addr_lo
                                        : I2cState::get_slave_addr;
  out_bit_ = true;
}

void Eeprom::i2c_stop() noexcept {
  i2c_state_ = I2cState::standby;
  i2c_bit_ = 0;
  out_bit_ = true;
}

void Eeprom::i2c_clock(bool sda) noexcept {
  switch (i2c_state_) {
  case I2cState::standby:
    break;

  case I2cState::get_slave_addr:
    i2c_shift_ = static_cast<std::uint8_t>((i2c_shift_ << 1) | (sda ? 1U : 0U));
    if (++i2c_bit_ == 8) {
      i2c_bit_ = 0;
      i2c_rw_ = (i2c_shift_ & 1U) != 0;
      if (spec_.addr_bytes == 1 && !spec_.i2c_7bit_word_addr) {
        // Upper address bits for 24C04/24C08/24C16 live in bits 3..1 of slave
        // byte.
        const std::uint16_t hi =
            static_cast<std::uint16_t>((i2c_shift_ >> 1) & 0x07U);
        i2c_addr_ =
            static_cast<std::uint16_t>((hi << 8) | (i2c_addr_ & 0x00FFU));
      }
      if (i2c_rw_) {
        i2c_next_after_ack_ = I2cState::read_data;
      } else {
        i2c_next_after_ack_ = spec_.addr_bytes >= 2
                                  ? I2cState::get_word_addr_hi
                                  : I2cState::get_word_addr_lo;
      }
      i2c_state_ = I2cState::send_ack;
      out_bit_ = false; // ACK (low)
    }
    break;

  case I2cState::get_word_addr_hi:
    i2c_shift_ = static_cast<std::uint8_t>((i2c_shift_ << 1) | (sda ? 1U : 0U));
    if (++i2c_bit_ == 8) {
      i2c_bit_ = 0;
      i2c_addr_ = static_cast<std::uint16_t>(
          (static_cast<std::uint16_t>(i2c_shift_) << 8) |
          (i2c_addr_ & 0x00FFU));
      i2c_next_after_ack_ = I2cState::get_word_addr_lo;
      i2c_state_ = I2cState::send_ack;
      out_bit_ = false;
    }
    break;

  case I2cState::get_word_addr_lo:
    i2c_shift_ = static_cast<std::uint8_t>((i2c_shift_ << 1) | (sda ? 1U : 0U));
    if (++i2c_bit_ == 8) {
      i2c_bit_ = 0;
      if (spec_.i2c_7bit_word_addr) {
        i2c_addr_ = static_cast<std::uint16_t>((i2c_shift_ >> 1) & 0x7FU);
        i2c_rw_ = (i2c_shift_ & 1U) != 0;
        i2c_next_after_ack_ =
            i2c_rw_ ? I2cState::read_data : I2cState::write_data;
      } else {
        i2c_addr_ =
            static_cast<std::uint16_t>((i2c_addr_ & 0xFF00U) | i2c_shift_);
        i2c_next_after_ack_ = I2cState::write_data;
      }
      i2c_state_ = I2cState::send_ack;
      out_bit_ = false;
    }
    break;

  case I2cState::write_data:
    i2c_shift_ = static_cast<std::uint8_t>((i2c_shift_ << 1) | (sda ? 1U : 0U));
    if (++i2c_bit_ == 8) {
      i2c_bit_ = 0;
      if (!image_.empty()) {
        const std::size_t idx = i2c_addr_ % image_.size();
        image_[idx] = i2c_shift_;
        dirty_ = true;
      }
      const std::uint16_t page_mask =
          spec_.page_size > 1 ? static_cast<std::uint16_t>(spec_.page_size - 1U)
                              : 0U;
      if (page_mask != 0) {
        i2c_addr_ = static_cast<std::uint16_t>((i2c_addr_ & ~page_mask) |
                                               ((i2c_addr_ + 1U) & page_mask));
      } else {
        i2c_addr_ = static_cast<std::uint16_t>(i2c_addr_ + 1U);
      }
      i2c_next_after_ack_ = I2cState::write_data;
      i2c_state_ = I2cState::send_ack;
      out_bit_ = false;
    }
    break;

  case I2cState::read_data: {
    const std::uint8_t val =
        image_.empty() ? 0xFF : image_[i2c_addr_ % image_.size()];
    out_bit_ = ((val >> (7U - i2c_bit_)) & 1U) != 0;
    if (++i2c_bit_ == 8) {
      i2c_bit_ = 0;
      i2c_addr_ = static_cast<std::uint16_t>(i2c_addr_ + 1U);
      i2c_state_ = I2cState::wait_ack_after_read;
    }
    break;
  }

  case I2cState::wait_ack_after_read:
    // Master drives SDA=0 to ACK (continue sequential read), SDA=1 to NACK.
    if (!sda) {
      i2c_state_ = I2cState::read_data;
      i2c_bit_ = 0;
      out_bit_ = true;
    } else {
      i2c_state_ = I2cState::standby;
      out_bit_ = true;
    }
    break;

  case I2cState::send_ack:
    // 9th SCL rising edge: keep ACK (out_bit_ = false) valid while SCL is high,
    // then transition on the falling edge of the 9th clock.
    i2c_state_ = I2cState::send_ack_wait_fall;
    break;

  case I2cState::send_ack_wait_fall:
    break;
  }
}

void Eeprom::drive_i2c(bool scl, bool sda) noexcept {
  // START: SDA falls while SCL stays high
  if (prev_scl_ && scl && prev_sda_ && !sda) {
    i2c_start();
  } else if (prev_scl_ && scl && !prev_sda_ && sda) {
    // STOP: SDA rises while SCL stays high
    i2c_stop();
  } else if (!prev_scl_ && scl) {
    // Rising edge of SCL: sample SDA or advance ACK phase
    i2c_clock(sda);
  } else if (prev_scl_ && !scl) {
    // Falling edge of SCL: transition out of 9th ACK pulse
    if (i2c_state_ == I2cState::send_ack_wait_fall) {
      i2c_state_ = i2c_next_after_ack_;
      i2c_bit_ = 0;
      if (i2c_state_ == I2cState::read_data) {
        const std::uint8_t val =
            image_.empty() ? 0xFF : image_[i2c_addr_ % image_.size()];
        out_bit_ = ((val >> 7U) & 1U) != 0;
      } else {
        out_bit_ = true;
      }
    }
  }
  prev_scl_ = scl;
  prev_sda_ = sda;
}

void Eeprom::drive_spi(bool cs_low, bool sck, bool mosi) noexcept {
  // cs_low == true means chip selected (/CS = 0)
  const bool cs_pin = !cs_low;
  if (cs_pin) {
    spi_state_ = SpiState::idle;
    spi_bit_ = 0;
    out_bit_ = true;
    prev_cs_ = true;
    prev_scl_ = sck;
    prev_sda_ = mosi;
    return;
  }

  if (prev_cs_ && !cs_pin) {
    // Falling edge of /CS starts an instruction
    spi_state_ = SpiState::opcode;
    spi_bit_ = 0;
    spi_shift_ = 0;
  }

  // Rising edge of SCK shifts in MOSI and prepares next MISO bit
  if (!prev_scl_ && sck) {
    spi_shift_ =
        static_cast<std::uint8_t>((spi_shift_ << 1) | (mosi ? 1U : 0U));
    switch (spi_state_) {
    case SpiState::idle:
      break;
    case SpiState::opcode:
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        switch (spi_shift_) {
        case 0x06: // WREN
          spi_wel_ = true;
          spi_state_ = SpiState::idle;
          break;
        case 0x04: // WRDI
          spi_wel_ = false;
          spi_state_ = SpiState::idle;
          break;
        case 0x05: // RDSR
          spi_out_shift_ = static_cast<std::uint8_t>(
              (spi_status_ & ~0x03U) | (spi_wel_ ? 0x02U : 0x00U));
          spi_state_ = SpiState::status_read;
          out_bit_ = (spi_out_shift_ & 0x80U) != 0;
          break;
        case 0x01: // WRSR
          spi_state_ = SpiState::status_write;
          break;
        case 0x03: // READ
          spi_addr_ = 0;
          spi_state_ =
              spec_.addr_bytes >= 2 ? SpiState::addr_hi : SpiState::addr_lo;
          break;
        case 0x02: // WRITE
          spi_addr_ = 0;
          spi_state_ =
              spec_.addr_bytes >= 2 ? SpiState::addr_hi : SpiState::addr_lo;
          spi_status_ |= 0x80U; // track write intent via opcode bit
          break;
        default:
          spi_state_ = SpiState::idle;
          break;
        }
      }
      break;
    case SpiState::addr_hi:
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        spi_addr_ = static_cast<std::uint16_t>(spi_shift_) << 8;
        spi_state_ = SpiState::addr_lo;
      }
      break;
    case SpiState::addr_lo:
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        spi_addr_ = static_cast<std::uint16_t>(spi_addr_ | spi_shift_);
        if ((spi_status_ & 0x80U) != 0) {
          spi_status_ &= ~0x80U;
          spi_state_ = SpiState::write_data;
        } else {
          spi_state_ = SpiState::read_data;
          spi_out_shift_ =
              image_.empty() ? 0xFF : image_[spi_addr_ % image_.size()];
          out_bit_ = (spi_out_shift_ & 0x80U) != 0;
        }
      }
      break;
    case SpiState::read_data:
      out_bit_ = (spi_out_shift_ & 0x80U) != 0;
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        spi_addr_ = static_cast<std::uint16_t>(spi_addr_ + 1U);
        spi_out_shift_ =
            image_.empty() ? 0xFF : image_[spi_addr_ % image_.size()];
      } else {
        spi_out_shift_ <<= 1;
      }
      break;
    case SpiState::write_data:
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        if (spi_wel_ && !image_.empty()) {
          image_[spi_addr_ % image_.size()] = spi_shift_;
          dirty_ = true;
        }
        spi_addr_ = static_cast<std::uint16_t>(spi_addr_ + 1U);
      }
      break;
    case SpiState::status_read:
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        spi_out_shift_ = static_cast<std::uint8_t>((spi_status_ & ~0x03U) |
                                                   (spi_wel_ ? 0x02U : 0x00U));
      } else {
        spi_out_shift_ <<= 1;
      }
      out_bit_ = (spi_out_shift_ & 0x80U) != 0;
      break;
    case SpiState::status_write:
      if (++spi_bit_ == 8) {
        spi_bit_ = 0;
        if (spi_wel_)
          spi_status_ = static_cast<std::uint8_t>(spi_shift_ & 0x0CU);
        spi_state_ = SpiState::idle;
      }
      break;
    }
  }

  prev_cs_ = cs_pin;
  prev_scl_ = sck;
  prev_sda_ = mosi;
}

void Eeprom::drive_microwire(bool cs_high, bool clk, bool di) noexcept {
  if (!cs_high) {
    mw_state_ = MwState::idle;
    mw_bit_ = 0;
    out_bit_ = true; // ready
    prev_cs_ = false;
    prev_scl_ = clk;
    prev_sda_ = di;
    return;
  }

  if (!prev_cs_ && cs_high) {
    mw_state_ = MwState::wait_start;
    mw_bit_ = 0;
    mw_shift_ = 0;
  }

  if (!prev_scl_ && clk) {
    switch (mw_state_) {
    case MwState::idle:
      break;
    case MwState::wait_start:
      if (di) {
        mw_state_ = MwState::opcode_addr;
        mw_bit_ = 0;
        mw_shift_ = 0;
      }
      break;
    case MwState::opcode_addr: {
      mw_shift_ = (mw_shift_ << 1) | (di ? 1U : 0U);
      const std::uint8_t total_bits =
          static_cast<std::uint8_t>(2U + spec_.mw_addr_bits);
      if (++mw_bit_ == total_bits) {
        mw_bit_ = 0;
        mw_op_ = static_cast<std::uint8_t>((mw_shift_ >> spec_.mw_addr_bits) &
                                           0x03U);
        const std::uint32_t addr_mask = (1U << spec_.mw_addr_bits) - 1U;
        mw_addr_ = static_cast<std::uint16_t>(mw_shift_ & addr_mask);
        if (mw_op_ == 0x00U) {
          const std::uint8_t sub =
              static_cast<std::uint8_t>(mw_addr_ >> (spec_.mw_addr_bits - 2U));
          if (sub == 0x03U)
            mw_ewen_ = true; // EWEN
          else if (sub == 0x00U)
            mw_ewen_ = false; // EWDS
          else if (sub == 0x02U && mw_ewen_ && !image_.empty()) {
            std::fill(image_.begin(), image_.end(), std::uint8_t{0xFF}); // ERAL
            dirty_ = true;
          }
          mw_state_ = MwState::idle;
        } else if (mw_op_ == 0x02U) { // READ
          mw_state_ = MwState::read_data;
          const std::size_t byte_idx = spec_.mw_16bit
                                           ? std::size_t{mw_addr_} * 2U
                                           : std::size_t{mw_addr_};
          if (spec_.mw_16bit && byte_idx + 1 < image_.size()) {
            mw_out_shift_ = static_cast<std::uint16_t>(
                (std::uint16_t{image_[byte_idx]} << 8) | image_[byte_idx + 1]);
            out_bit_ = (mw_out_shift_ & 0x8000U) != 0;
          } else if (!spec_.mw_16bit && byte_idx < image_.size()) {
            mw_out_shift_ = image_[byte_idx];
            out_bit_ = (mw_out_shift_ & 0x0080U) != 0;
          } else {
            mw_out_shift_ = 0xFFFF;
            out_bit_ = true;
          }
        } else if (mw_op_ == 0x01U) { // WRITE
          mw_state_ = MwState::write_data;
          mw_shift_ = 0;
        } else if (mw_op_ == 0x03U) { // ERASE
          if (mw_ewen_ && !image_.empty()) {
            const std::size_t byte_idx = spec_.mw_16bit
                                             ? std::size_t{mw_addr_} * 2U
                                             : std::size_t{mw_addr_};
            if (spec_.mw_16bit && byte_idx + 1 < image_.size()) {
              image_[byte_idx] = 0xFF;
              image_[byte_idx + 1] = 0xFF;
              dirty_ = true;
            } else if (!spec_.mw_16bit && byte_idx < image_.size()) {
              image_[byte_idx] = 0xFF;
              dirty_ = true;
            }
          }
          mw_state_ = MwState::idle;
        }
      }
      break;
    }
    case MwState::read_data: {
      const std::uint8_t width = spec_.mw_16bit ? 16U : 8U;
      if (++mw_bit_ == width) {
        mw_bit_ = 0;
        mw_addr_ = static_cast<std::uint16_t>(mw_addr_ + 1U);
        const std::size_t byte_idx =
            spec_.mw_16bit ? std::size_t{mw_addr_} * 2U : std::size_t{mw_addr_};
        if (spec_.mw_16bit && byte_idx + 1 < image_.size()) {
          mw_out_shift_ = static_cast<std::uint16_t>(
              (std::uint16_t{image_[byte_idx]} << 8) | image_[byte_idx + 1]);
        } else if (!spec_.mw_16bit && byte_idx < image_.size()) {
          mw_out_shift_ = image_[byte_idx];
        } else {
          mw_out_shift_ = 0xFFFF;
        }
      } else {
        mw_out_shift_ <<= 1;
      }
      out_bit_ = spec_.mw_16bit ? ((mw_out_shift_ & 0x8000U) != 0)
                                : ((mw_out_shift_ & 0x0080U) != 0);
      break;
    }
    case MwState::write_data: {
      mw_shift_ = (mw_shift_ << 1) | (di ? 1U : 0U);
      const std::uint8_t width = spec_.mw_16bit ? 16U : 8U;
      if (++mw_bit_ == width) {
        mw_bit_ = 0;
        if (mw_ewen_ && !image_.empty()) {
          const std::size_t byte_idx = spec_.mw_16bit
                                           ? std::size_t{mw_addr_} * 2U
                                           : std::size_t{mw_addr_};
          if (spec_.mw_16bit && byte_idx + 1 < image_.size()) {
            image_[byte_idx] =
                static_cast<std::uint8_t>((mw_shift_ >> 8) & 0xFFU);
            image_[byte_idx + 1] = static_cast<std::uint8_t>(mw_shift_ & 0xFFU);
            dirty_ = true;
          } else if (!spec_.mw_16bit && byte_idx < image_.size()) {
            image_[byte_idx] = static_cast<std::uint8_t>(mw_shift_ & 0xFFU);
            dirty_ = true;
          }
        }
        mw_state_ = MwState::idle;
      }
      break;
    }
    }
  }

  prev_cs_ = cs_high;
  prev_scl_ = clk;
  prev_sda_ = di;
}

bool Eeprom::load_srm(std::span<const std::uint8_t> file) {
  if (!present() || file.size() > image_.size())
    return false;
  std::fill(image_.begin(), image_.end(), std::uint8_t{0xFF});
  std::copy(file.begin(), file.end(), image_.begin());
  dirty_ = false;
  return true;
}

bool Eeprom::save_atomic(const std::string &path) {
  if (!present())
    return false;
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

void Eeprom::save(StateWriter &w) const {
  w.bytes(image_);
  w.boolean(dirty_);
  w.boolean(out_bit_);
  w.boolean(prev_scl_);
  w.boolean(prev_sda_);
  w.boolean(prev_cs_);
  w.u8(static_cast<std::uint8_t>(i2c_state_));
  w.u8(static_cast<std::uint8_t>(i2c_next_after_ack_));
  w.u8(i2c_bit_);
  w.u8(i2c_shift_);
  w.u16(i2c_addr_);
  w.boolean(i2c_rw_);
  w.u8(static_cast<std::uint8_t>(spi_state_));
  w.u8(spi_bit_);
  w.u8(spi_shift_);
  w.u8(spi_out_shift_);
  w.u8(spi_status_);
  w.boolean(spi_wel_);
  w.u16(spi_addr_);
  w.u8(static_cast<std::uint8_t>(mw_state_));
  w.u8(mw_bit_);
  w.u32(mw_shift_);
  w.u16(mw_out_shift_);
  w.u16(mw_addr_);
  w.boolean(mw_ewen_);
  w.u8(mw_op_);
}

void Eeprom::load(StateReader &r) {
  r.bytes(image_);
  dirty_ = r.boolean();
  out_bit_ = r.boolean();
  prev_scl_ = r.boolean();
  prev_sda_ = r.boolean();
  prev_cs_ = r.boolean();
  i2c_state_ = static_cast<I2cState>(r.u8());
  i2c_next_after_ack_ = static_cast<I2cState>(r.u8());
  i2c_bit_ = r.u8();
  i2c_shift_ = r.u8();
  i2c_addr_ = r.u16();
  i2c_rw_ = r.boolean();
  spi_state_ = static_cast<SpiState>(r.u8());
  spi_bit_ = r.u8();
  spi_shift_ = r.u8();
  spi_out_shift_ = r.u8();
  spi_status_ = r.u8();
  spi_wel_ = r.boolean();
  spi_addr_ = r.u16();
  mw_state_ = static_cast<MwState>(r.u8());
  mw_bit_ = r.u8();
  mw_shift_ = r.u32();
  mw_out_shift_ = r.u16();
  mw_addr_ = r.u16();
  mw_ewen_ = r.boolean();
  mw_op_ = r.u8();
}

std::uint16_t JCart::read16() const noexcept {
  auto encode_port = [&](std::uint16_t mask) -> std::uint8_t {
    if (th_) {
      std::uint8_t v = 0x3FU;
      if (mask & kPadUp)
        v &= ~0x01U;
      if (mask & kPadDown)
        v &= ~0x02U;
      if (mask & kPadLeft)
        v &= ~0x04U;
      if (mask & kPadRight)
        v &= ~0x08U;
      if (mask & kPadB)
        v &= ~0x10U;
      if (mask & kPadC)
        v &= ~0x20U;
      return v;
    }
    std::uint8_t v = 0x33U;
    if (mask & kPadUp)
      v &= ~0x01U;
    if (mask & kPadDown)
      v &= ~0x02U;
    if (mask & kPadA)
      v &= ~0x10U;
    if (mask & kPadStart)
      v &= ~0x20U;
    return v;
  };
  const std::uint8_t p3 = encode_port(pads_[0]);
  const std::uint8_t p4 = encode_port(pads_[1]);
  return static_cast<std::uint16_t>((std::uint16_t{p4} << 8) | p3);
}

void JCart::write16(std::uint16_t value) noexcept { th_ = (value & 1U) != 0; }

void JCart::save(StateWriter &w) const {
  w.boolean(present_);
  w.boolean(th_);
  w.u16(pads_[0]);
  w.u16(pads_[1]);
}

void JCart::load(StateReader &r) {
  present_ = r.boolean();
  th_ = r.boolean();
  pads_[0] = r.u16();
  pads_[1] = r.u16();
}

void FlashCart::init(Spec spec, std::span<const std::uint8_t> initial_rom) {
  spec_ = spec;
  if (spec_.present) {
    image_.assign(initial_rom.begin(), initial_rom.end());
  } else {
    image_.clear();
  }
  mode_ = Mode::read;
  dirty_ = false;
}

std::uint8_t FlashCart::read8(std::uint32_t address) const noexcept {
  if (!spec_.present || image_.empty())
    return 0xFF;
  if (mode_ == Mode::autoselect) {
    return (address & 1U) == 0 ? spec_.manufacturer_id : spec_.device_id;
  }
  return address < image_.size() ? image_[address] : 0xFF;
}

void FlashCart::write8(std::uint32_t address, std::uint8_t value) noexcept {
  if (!spec_.present || image_.empty())
    return;
  const std::uint32_t cmd_addr = address & 0x7FFFU;
  if (value == 0xF0U && mode_ != Mode::program) {
    mode_ = Mode::read;
    return;
  }
  switch (mode_) {
  case Mode::read:
  case Mode::autoselect:
    if (cmd_addr == 0x5555U && value == 0xAAU)
      mode_ = Mode::unlock1;
    break;
  case Mode::unlock1:
    mode_ =
        (cmd_addr == 0x2AAAU && value == 0x55U) ? Mode::unlock2 : Mode::read;
    break;
  case Mode::unlock2:
    if (cmd_addr == 0x5555U) {
      if (value == 0x90U)
        mode_ = Mode::autoselect;
      else if (value == 0xA0U)
        mode_ = Mode::program;
      else if (value == 0x80U)
        mode_ = Mode::erase_cmd;
      else
        mode_ = Mode::read;
    } else {
      mode_ = Mode::read;
    }
    break;
  case Mode::program:
    if (address < image_.size()) {
      image_[address] &= value;
      dirty_ = true;
    }
    mode_ = Mode::read;
    break;
  case Mode::erase_cmd:
    mode_ = (cmd_addr == 0x5555U && value == 0xAAU) ? Mode::erase_unlock1
                                                    : Mode::read;
    break;
  case Mode::erase_unlock1:
    mode_ = (cmd_addr == 0x2AAAU && value == 0x55U) ? Mode::erase_unlock2
                                                    : Mode::read;
    break;
  case Mode::erase_unlock2:
    if (value == 0x10U && cmd_addr == 0x5555U) {
      std::fill(image_.begin(), image_.end(), std::uint8_t{0xFF});
      dirty_ = true;
    } else if (value == 0x30U && spec_.sector_size > 0) {
      const std::size_t base =
          (address / spec_.sector_size) * spec_.sector_size;
      const std::size_t end = std::min(image_.size(), base + spec_.sector_size);
      if (base < end) {
        std::fill(image_.begin() + base, image_.begin() + end,
                  std::uint8_t{0xFF});
        dirty_ = true;
      }
    }
    mode_ = Mode::read;
    break;
  }
}

void FlashCart::save(StateWriter &w) const {
  w.boolean(spec_.present);
  w.bytes(image_);
  w.u8(static_cast<std::uint8_t>(mode_));
  w.boolean(dirty_);
}

void FlashCart::load(StateReader &r) {
  spec_.present = r.boolean();
  r.bytes(image_);
  mode_ = static_cast<Mode>(r.u8());
  dirty_ = r.boolean();
}

SpecialCartInfo detect_special_cart(std::span<const std::uint8_t> rom) {
  SpecialCartInfo info;
  if (rom.size() < 0x190)
    return info;
  const std::string_view serial(
      reinterpret_cast<const char *>(rom.data() + 0x180), 14);
  if (serial.find("T-120096") !=
          std::string_view::npos || // Mega Man: The Wily Wars
      serial.find("G-4060") !=
          std::string_view::npos) { // Wonder Boy in Monster World
    info.eeprom.protocol = EepromProtocol::i2c;
    info.eeprom.size_bytes = 128;
    info.eeprom.page_size = 4;
    info.eeprom.i2c_7bit_word_addr = true;
  } else if (serial.find("T-50176") !=
                 std::string_view::npos || // NHLPA Hockey 93
             serial.find("T-50396") !=
                 std::string_view::npos) { // Rings of Power
    info.eeprom.protocol = EepromProtocol::i2c;
    info.eeprom.size_bytes = 128;
    info.eeprom.page_size = 4;
    info.eeprom.i2c_7bit_word_addr = true;
  } else if (serial.find("T-120146") !=
                 std::string_view::npos || // Micro Machines 2
             serial.find("T-120066") !=
                 std::string_view::npos) { // Pete Sampras Tennis
    info.jcart = true;
  }
  return info;
}

} // namespace tessera::md
