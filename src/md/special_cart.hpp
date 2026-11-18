// Special cartridge hardware for Mega Drive (spec 008, Phase 5A):
// - EEPROM (I2C 24Cxx, SPI 25xx/M95xx, Microwire 93Cxx), .srm byte-compatible
// - Codemasters J-Cart (extra controller ports 3 and 4 at $38FFFE)
// - Flash memory cartridges (AMD/SST command state machine)
// - Lock-On / Sonic & Knuckles mapper configuration
#pragma once
#include "md/state_io.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace tessera::md {

enum class EepromProtocol : std::uint8_t { none = 0, i2c, spi, microwire };

struct EepromPinMap {
  std::uint32_t sda_in_addr = 0x200001;
  std::uint32_t sda_out_addr = 0x200001;
  std::uint32_t scl_addr = 0x200001;
  std::uint32_t cs_addr =
      0x200001; ///< used by SPI (active low) & Microwire (active high)
  std::uint8_t sda_in_bit = 0;  ///< bit written by CPU (MOSI / DI / SDA out)
  std::uint8_t sda_out_bit = 0; ///< bit read by CPU (MISO / DO / SDA in)
  std::uint8_t scl_bit = 1;
  std::uint8_t cs_bit = 2;
};

struct EepromSpec {
  EepromProtocol protocol = EepromProtocol::none;
  std::uint32_t size_bytes = 0;
  std::uint16_t page_size = 1; ///< I2C page write size (4, 8, 16, 32...)
  std::uint8_t addr_bytes =
      1; ///< 1 for 24C01..24C16 / 25040, 2 for 24C64 / 25128
  std::uint8_t mw_addr_bits = 6; ///< 6 for 93C46 (x16), 8 for 93C56/66
  bool mw_16bit = true;          ///< Microwire data width (16-bit or 8-bit)
  bool i2c_7bit_word_addr =
      false; ///< true for 24C01 (7-bit word addr + R/W in first byte)
  EepromPinMap pins;
};

class Eeprom {
public:
  explicit Eeprom(const EepromSpec &spec = {});

  bool present() const noexcept {
    return spec_.protocol != EepromProtocol::none && !image_.empty();
  }
  EepromProtocol protocol() const noexcept { return spec_.protocol; }
  const EepromSpec &spec() const noexcept { return spec_; }
  std::size_t image_size() const noexcept { return image_.size(); }
  bool dirty() const noexcept { return dirty_; }

  bool contains_addr(std::uint32_t address) const noexcept;
  std::uint8_t read8(std::uint32_t address) const noexcept;
  void write8(std::uint32_t address, std::uint8_t value) noexcept;

  /// Direct line-level drive (useful for unit tests and custom board wiring).
  void drive_i2c(bool scl, bool sda) noexcept;
  void drive_spi(bool cs_low, bool sck, bool mosi) noexcept;
  void drive_microwire(bool cs_high, bool clk, bool di) noexcept;
  bool data_out() const noexcept { return out_bit_; }

  /// .srm persistence (byte-compatible).
  bool load_srm(std::span<const std::uint8_t> file);
  std::vector<std::uint8_t> to_srm() const { return image_; }
  bool save_atomic(const std::string &path);

  void save(StateWriter &w) const;
  void load(StateReader &r);

private:
  void i2c_start() noexcept;
  void i2c_stop() noexcept;
  void i2c_clock(bool sda) noexcept;

  enum class I2cState : std::uint8_t {
    standby = 0,
    get_slave_addr,
    get_word_addr_hi,
    get_word_addr_lo,
    read_data,
    write_data,
    wait_ack_after_read,
    send_ack,
    send_ack_wait_fall
  };

  enum class SpiState : std::uint8_t {
    idle = 0,
    opcode,
    addr_hi,
    addr_lo,
    read_data,
    write_data,
    status_read,
    status_write
  };

  enum class MwState : std::uint8_t {
    idle = 0,
    wait_start,
    opcode_addr,
    read_data,
    write_data
  };

  EepromSpec spec_;
  std::vector<std::uint8_t> image_;
  bool dirty_ = false;
  bool out_bit_ = true;

  // Latched input lines
  bool prev_scl_ = true;
  bool prev_sda_ = true;
  bool prev_cs_ = true;

  // I2C state
  I2cState i2c_state_ = I2cState::standby;
  I2cState i2c_next_after_ack_ = I2cState::standby;
  std::uint8_t i2c_bit_ = 0;
  std::uint8_t i2c_shift_ = 0;
  std::uint16_t i2c_addr_ = 0;
  bool i2c_rw_ = false;

  // SPI state
  SpiState spi_state_ = SpiState::idle;
  std::uint8_t spi_bit_ = 0;
  std::uint8_t spi_shift_ = 0;
  std::uint8_t spi_out_shift_ = 0;
  std::uint8_t spi_status_ = 0;
  bool spi_wel_ = false;
  std::uint16_t spi_addr_ = 0;

  // Microwire state
  MwState mw_state_ = MwState::idle;
  std::uint8_t mw_bit_ = 0;
  std::uint32_t mw_shift_ = 0;
  std::uint16_t mw_out_shift_ = 0;
  std::uint16_t mw_addr_ = 0;
  bool mw_ewen_ = false;
  std::uint8_t mw_op_ = 0;
};

/// Codemasters J-Cart extra controller ports (ports 3 and 4 at
/// $38FFFE-$3FFFFF).
class JCart {
public:
  bool present() const noexcept { return present_; }
  void set_present(bool p) noexcept { present_ = p; }

  void set_pad(unsigned extra_port, std::uint16_t pressed_mask) noexcept {
    if (extra_port < 2)
      pads_[extra_port] = pressed_mask;
  }

  static bool is_jcart_addr(std::uint32_t address) noexcept {
    const std::uint32_t a = address & 0xFFFFFFU;
    return a >= 0x380000U && a < 0x400000U;
  }

  std::uint16_t read16() const noexcept;
  void write16(std::uint16_t value) noexcept;

  void save(StateWriter &w) const;
  void load(StateReader &r);

private:
  bool present_ = false;
  bool th_ = true;
  std::uint16_t pads_[2]{0, 0};
};

/// AMD/SST-compatible Flash cartridge state machine.
class FlashCart {
public:
  struct Spec {
    bool present = false;
    std::uint8_t manufacturer_id = 0xBF; ///< SST
    std::uint8_t device_id = 0xB7;       ///< SST39SF040 (512 KiB)
    std::uint32_t sector_size = 4096;
  };

  FlashCart() = default;
  void init(Spec spec, std::span<const std::uint8_t> initial_rom);

  bool present() const noexcept { return spec_.present; }
  std::uint8_t read8(std::uint32_t address) const noexcept;
  void write8(std::uint32_t address, std::uint8_t value) noexcept;

  std::span<const std::uint8_t> image() const noexcept { return image_; }
  bool dirty() const noexcept { return dirty_; }

  void save(StateWriter &w) const;
  void load(StateReader &r);

private:
  enum class Mode : std::uint8_t {
    read = 0,
    unlock1,
    unlock2,
    autoselect,
    program,
    erase_cmd,
    erase_unlock1,
    erase_unlock2
  };

  Spec spec_{};
  std::vector<std::uint8_t> image_;
  Mode mode_ = Mode::read;
  bool dirty_ = false;
};

struct SpecialCartInfo {
  EepromSpec eeprom;
  bool jcart = false;
  FlashCart::Spec flash;
};

/// Detects special cartridge hardware from a plain Mega Drive ROM header.
SpecialCartInfo detect_special_cart(std::span<const std::uint8_t> rom);

} // namespace tessera::md
