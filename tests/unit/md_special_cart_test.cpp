#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "md/bus.hpp"
#include "md/special_cart.hpp"
#include "md/system.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace tessera::md;

namespace {

class DummyDevices : public BusDevices {
public:
  std::uint16_t read(Target, std::uint32_t, bool) override { return 0xFFFF; }
  void write(Target, std::uint32_t, std::uint16_t, bool) override {}
};

void i2c_write_bit(Bus &bus, bool sda) {
  const std::uint8_t sda_b = sda ? 0x01U : 0x00U;
  bus.write8(0x200001, sda_b);                               // SCL=0, SDA=sda
  bus.write8(0x200001, static_cast<uint8_t>(0x02U | sda_b)); // SCL=1
  bus.write8(0x200001, sda_b);                               // SCL=0
}

bool i2c_read_bit(Bus &bus) {
  bus.write8(0x200001, 0x01U); // release SDA, SCL=0
  bus.write8(0x200001, 0x03U); // SCL=1
  const bool bit = (bus.read8(0x200001) & 0x01U) != 0;
  bus.write8(0x200001, 0x01U); // SCL=0
  return bit;
}

void i2c_start(Bus &bus) {
  bus.write8(0x200001, 0x03U); // SCL=1, SDA=1
  bus.write8(0x200001, 0x02U); // SCL=1, SDA=0 (START)
  bus.write8(0x200001, 0x00U); // SCL=0
}

void i2c_stop(Bus &bus) {
  bus.write8(0x200001, 0x00U); // SCL=0, SDA=0
  bus.write8(0x200001, 0x02U); // SCL=1, SDA=0
  bus.write8(0x200001, 0x03U); // SCL=1, SDA=1 (STOP)
}

bool i2c_send_byte(Bus &bus, std::uint8_t b) {
  for (int i = 7; i >= 0; --i)
    i2c_write_bit(bus, ((b >> i) & 1U) != 0);
  return !i2c_read_bit(bus); // ACK is low
}

std::uint8_t i2c_recv_byte(Bus &bus, bool ack) {
  std::uint8_t v = 0;
  for (int i = 7; i >= 0; --i)
    if (i2c_read_bit(bus))
      v |= static_cast<std::uint8_t>(1U << i);
  i2c_write_bit(bus, !ack); // 0 = ACK, 1 = NACK
  return v;
}

void spi_xfer_byte(Eeprom &e, std::uint8_t out, std::uint8_t &in) {
  in = 0;
  for (int i = 7; i >= 0; --i) {
    const bool mosi = ((out >> i) & 1U) != 0;
    e.drive_spi(true, false, mosi);
    e.drive_spi(true, true, mosi);
    if (e.data_out())
      in |= static_cast<std::uint8_t>(1U << i);
  }
}

void mw_send_bits(Eeprom &e, std::uint32_t val, int count) {
  for (int i = count - 1; i >= 0; --i) {
    const bool di = ((val >> i) & 1U) != 0;
    e.drive_microwire(true, false, di);
    e.drive_microwire(true, true, di);
  }
}

std::uint16_t mw_recv_bits(Eeprom &e, int count) {
  std::uint16_t v = 0;
  for (int i = count - 1; i >= 0; --i) {
    if (e.data_out())
      v |= static_cast<std::uint16_t>(1U << i);
    e.drive_microwire(true, false, false);
    e.drive_microwire(true, true, false);
  }
  return v;
}

} // namespace

TEST_CASE("EEPROM I2C 24C02 write, sequential read, srm and savestate") {
  EepromSpec spec;
  spec.protocol = EepromProtocol::i2c;
  spec.size_bytes = 256;
  spec.page_size = 8;
  spec.addr_bytes = 1;
  Eeprom eeprom(spec);
  DummyDevices dev;
  Bus bus(std::vector<std::uint8_t>(0x100000, 0x00), dev);
  bus.attach_eeprom(&eeprom);

  i2c_start(bus);
  CHECK(i2c_send_byte(bus, 0xA0)); // write
  CHECK(i2c_send_byte(bus, 0x10)); // addr 0x10
  CHECK(i2c_send_byte(bus, 0x42));
  CHECK(i2c_send_byte(bus, 0x99));
  i2c_stop(bus);

  CHECK(eeprom.dirty());
  CHECK(eeprom.to_srm()[0x10] == 0x42);
  CHECK(eeprom.to_srm()[0x11] == 0x99);

  // Random sequential read from 0x10
  i2c_start(bus);
  CHECK(i2c_send_byte(bus, 0xA0));
  CHECK(i2c_send_byte(bus, 0x10));
  i2c_start(bus);
  CHECK(i2c_send_byte(bus, 0xA1));
  CHECK(i2c_recv_byte(bus, true) == 0x42);
  CHECK(i2c_recv_byte(bus, false) == 0x99);
  i2c_stop(bus);

  // Savestate round-trip
  std::vector<std::uint8_t> state_buf;
  StateWriter w(state_buf);
  eeprom.save(w);
  Eeprom restored(spec);
  StateReader r(state_buf);
  restored.load(r);
  CHECK(restored.to_srm() == eeprom.to_srm());
  CHECK(restored.dirty());

  // Byte-compatible .srm reload clears dirty flag
  const auto srm = eeprom.to_srm();
  Eeprom from_srm(spec);
  CHECK(from_srm.load_srm(srm));
  CHECK(!from_srm.dirty());
  CHECK(from_srm.to_srm()[0x10] == 0x42);
}

TEST_CASE("EEPROM SPI 25xx WREN, WRITE, READ") {
  EepromSpec spec;
  spec.protocol = EepromProtocol::spi;
  spec.size_bytes = 512;
  spec.addr_bytes = 2;
  Eeprom eeprom(spec);

  std::uint8_t dummy = 0;
  // WREN
  eeprom.drive_spi(false, false, false);
  spi_xfer_byte(eeprom, 0x06, dummy);
  eeprom.drive_spi(false, false, false);

  // WRITE 0xAB at 0x0123
  spi_xfer_byte(eeprom, 0x02, dummy);
  spi_xfer_byte(eeprom, 0x01, dummy);
  spi_xfer_byte(eeprom, 0x23, dummy);
  spi_xfer_byte(eeprom, 0xAB, dummy);
  eeprom.drive_spi(false, false, false);

  CHECK(eeprom.to_srm()[0x0123] == 0xAB);

  // READ from 0x0123
  std::uint8_t got = 0;
  spi_xfer_byte(eeprom, 0x03, dummy);
  spi_xfer_byte(eeprom, 0x01, dummy);
  spi_xfer_byte(eeprom, 0x23, dummy);
  spi_xfer_byte(eeprom, 0x00, got);
  eeprom.drive_spi(false, false, false);
  CHECK(got == 0xAB);
}

TEST_CASE("EEPROM Microwire 93C46 EWEN, WRITE, READ 16-bit") {
  EepromSpec spec;
  spec.protocol = EepromProtocol::microwire;
  spec.size_bytes = 128;
  spec.mw_addr_bits = 6;
  spec.mw_16bit = true;
  Eeprom eeprom(spec);

  // EWEN: start=1, op=00, addr=11xxxx (0b1_00_110000 = 0x130)
  eeprom.drive_microwire(false, false, false);
  mw_send_bits(eeprom, 0x130U, 9);
  eeprom.drive_microwire(false, false, false);

  // WRITE: start=1, op=01, addr=0x05, data=0xCAFE
  mw_send_bits(eeprom, (1U << 8) | (1U << 6) | 0x05U, 9);
  mw_send_bits(eeprom, 0xCAFEU, 16);
  eeprom.drive_microwire(false, false, false);

  CHECK(eeprom.to_srm()[0x05 * 2] == 0xCA);
  CHECK(eeprom.to_srm()[0x05 * 2 + 1] == 0xFE);

  // READ: start=1, op=10, addr=0x05
  mw_send_bits(eeprom, (1U << 8) | (2U << 6) | 0x05U, 9);
  const std::uint16_t got = mw_recv_bits(eeprom, 16);
  eeprom.drive_microwire(false, false, false);
  CHECK(got == 0xCAFEU);
}

TEST_CASE("J-Cart ports 3 and 4 via Bus") {
  JCart jcart;
  jcart.set_present(true);
  jcart.set_pad(0, kPadUp | kPadC);
  jcart.set_pad(1, kPadA | kPadStart);

  DummyDevices dev;
  Bus bus(std::vector<std::uint8_t>(0x400000, 0x00), dev);
  bus.attach_jcart(&jcart);

  // TH=1: reads Up/Down/Left/Right/B/C (active low)
  bus.write16(0x38FFFE, 0x0001);
  const std::uint16_t th1 = bus.read16(0x38FFFE);
  CHECK((th1 & 0x003FU) == (0x3FU & ~(0x01U | 0x20U))); // port 3: Up+C
  CHECK(((th1 >> 8) & 0x003FU) == 0x3FU); // port 4: none pressed in TH=1

  // TH=0: reads Up/Down/A/Start
  bus.write16(0x38FFFE, 0x0000);
  const std::uint16_t th0 = bus.read16(0x38FFFE);
  CHECK(((th0 >> 8) & 0x003FU) ==
        (0x33U & ~(0x10U | 0x20U))); // port 4: A+Start
}

TEST_CASE("FlashCart AMD/SST program, autoselect and sector erase via Bus") {
  FlashCart flash;
  std::vector<std::uint8_t> img(8192, 0xFF);
  img[0] = 0x12;
  flash.init({true, 0xBF, 0xB7, 4096}, img);

  DummyDevices dev;
  Bus bus(std::vector<std::uint8_t>(8192, 0x00), dev);
  bus.attach_flash(&flash);

  CHECK(bus.read8(0) == 0x12);

  // Autoselect ID
  bus.write8(0x5555, 0xAA);
  bus.write8(0x2AAA, 0x55);
  bus.write8(0x5555, 0x90);
  CHECK(bus.read8(0) == 0xBF);
  CHECK(bus.read8(1) == 0xB7);
  bus.write8(0, 0xF0);
  CHECK(bus.read8(0) == 0x12);

  // Program byte at 0x1200 (sector 1)
  bus.write8(0x5555, 0xAA);
  bus.write8(0x2AAA, 0x55);
  bus.write8(0x5555, 0xA0);
  bus.write8(0x1200, 0x5A);
  CHECK(bus.read8(0x1200) == 0x5A);
  CHECK(flash.dirty());

  // Sector erase sector 1 (0x1000..0x1FFF), sector 0 untouched
  bus.write8(0x5555, 0xAA);
  bus.write8(0x2AAA, 0x55);
  bus.write8(0x5555, 0x80);
  bus.write8(0x5555, 0xAA);
  bus.write8(0x2AAA, 0x55);
  bus.write8(0x1200, 0x30);
  CHECK(bus.read8(0x1200) == 0xFF);
  CHECK(bus.read8(0) == 0x12);
}

TEST_CASE("Lock-On Sonic & Knuckles + Sonic 2 UPMEM and Sonic 3 mapping") {
  std::vector<std::uint8_t> sk(0x240000, 0x11);
  sk[0x200000] = 0x77; // UPMEM patch byte
  std::vector<std::uint8_t> s2(0x100000, 0x22);

  DummyDevices dev;
  Bus bus(sk, dev, Mapper::lock_on_s2);
  bus.set_locked_rom(s2);

  CHECK(bus.read8(0x000100) == 0x11);
  CHECK(bus.read8(0x200100) == 0x22);
  CHECK(bus.read8(0x300000) == 0x22);
  // Enable UPMEM patch via $A130F1 bit 0
  bus.write8(0xA130F1, 0x01);
  CHECK(bus.read8(0x300000) == 0x77);
}
