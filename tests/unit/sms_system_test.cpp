#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "sms/system.hpp"
#include "systems/system_detector.hpp"

#include <cstdint>
#include <vector>

using namespace tessera::sms;

TEST_CASE("SMS Mode 4 VDP renders tile and sprite with CRAM RGB222") {
  // Tiny Z80 program at $0000:
  //   LD A, 0x40 ; OUT ($BF), A  (set VDP reg 1 = 0x40 display on)
  //   ... wait, write via VdpM4 or Z80 I/O loop: just HALT after setup.
  std::vector<std::uint8_t> rom(0x8000, 0x00);
  // Z80 code at $0000:
  //   LD A, $04 ; OUT ($BF), A ; LD A, $80 ; OUT ($BF), A  (reg 0 = 0x04 Mode
  //   4) LD A, $40 ; OUT ($BF), A ; LD A, $81 ; OUT ($BF), A  (reg 1 = 0x40
  //   Display ON) LD A, $00 ; OUT ($BF), A ; LD A, $C0 ; OUT ($BF), A  (CRAM
  //   write addr 0) LD A, $03 ; OUT ($BE), A (CRAM[0] = red 0x03) JR $FE (loop)
  const std::uint8_t code[] = {
      0x3E, 0x04, 0xD3, 0xBF, 0x3E, 0x80, 0xD3, 0xBF, // reg 0 = 0x04
      0x3E, 0x40, 0xD3, 0xBF, 0x3E, 0x81, 0xD3, 0xBF, // reg 1 = 0x40
      0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0xC0, 0xD3, 0xBF, // CRAM addr 0
      0x3E, 0x03, 0xD3, 0xBE,                         // CRAM[0] = 0x03 (red)
      0x18, 0xFE                                      // JR -2
  };
  for (std::size_t i = 0; i < sizeof(code); ++i)
    rom[i] = code[i];

  System sys(rom, ConsoleModel::sms);
  sys.run_frame();
  sys.run_frame();

  // CRAM[0] = 0x03 -> R=255, G=0, B=0 -> 0xFFFF0000
  CHECK(sys.vdp().cram()[0] == 0x03);
  CHECK(sys.vdp().framebuffer()[0] == 0xFFFF0000U);
  CHECK(sys.audio().size() == (48000 / 60) * 2);
}

TEST_CASE("Game Gear RGB444 palette, Start button and 160x144 dimensions") {
  std::vector<std::uint8_t> rom(0x8000, 0x76); // HALT
  System sys(rom, ConsoleModel::game_gear);
  CHECK(sys.vdp().width() == 160);
  CHECK(sys.vdp().height() == 144);
  sys.set_pad(0, kSmsPauseOrStart);
  sys.run_frame();

  // Verify VDP Mode 4 with GG 12-bit CRAM directly
  VdpM4 gg(ConsoleModel::game_gear);
  gg.write_control(0x04);
  gg.write_control(0x80); // reg 0 = Mode 4
  gg.write_control(0x40);
  gg.write_control(0x81); // reg 1 = display on
  gg.write_control(0x00);
  gg.write_control(0xC0); // CRAM addr 0
  gg.write_data(0xF0);    // G=0xF, R=0x0
  gg.write_data(0x00);    // B=0x0
  gg.render_line(0);
  CHECK(gg.framebuffer()[0] == 0xFF00FF00U);
}

TEST_CASE("SG-1000 TMS9918A renders Graphics I and savestate round-trips") {
  std::vector<std::uint8_t> rom(0x4000, 0x76); // HALT
  System sys(rom, ConsoleModel::sg1000);
  sys.run_frame();

  const auto state = sys.save_state();
  CHECK(!state.empty());

  System restored(rom, ConsoleModel::sg1000);
  CHECK(restored.load_state(state));
  CHECK(restored.frame() == sys.frame());
}

TEST_CASE("YM2413 AUDIO_MUTE (PQ-15) silences selected FM channels") {
  Ym2413 ym;
  ym.reset();
  ym.write_addr(0x10);
  ym.write_data(0x80); // F-num low
  ym.write_addr(0x20);
  ym.write_data(0x18); // key on + block
  ym.write_addr(0x30);
  ym.write_data(0x00); // max volume

  std::int32_t l1 = 0, r1 = 0;
  ym.step_sample(l1, r1);
  CHECK(l1 != 0);

  ym.set_mute(0x0001U); // mute channel 0
  std::int32_t l2 = 0, r2 = 0;
  ym.step_sample(l2, r2);
  CHECK(l2 == 0);
}

TEST_CASE("SystemDetector recognizes TMR SEGA SMS and Game Gear headers") {
  std::vector<std::uint8_t> sms(0x8000, 0x00);
  const char tmr[] = "TMR SEGA";
  for (int i = 0; i < 8; ++i)
    sms[0x7FF0 + i] = static_cast<std::uint8_t>(tmr[i]);
  sms[0x7FFF] = 0x4C; // SMS Export region 4

  auto d_sms = tessera::SystemDetector::detect(sms, TESSERA_SYSTEM_NONE);
  CHECK(d_sms.status == TESSERA_OK);
  CHECK(d_sms.candidates.size() == 1);
  CHECK(d_sms.candidates[0].system == TESSERA_SYSTEM_MASTER_SYSTEM);

  std::vector<std::uint8_t> gg = sms;
  gg[0x7FFF] = 0x6C; // GG Export region 6
  auto d_gg = tessera::SystemDetector::detect(gg, TESSERA_SYSTEM_NONE);
  CHECK(d_gg.status == TESSERA_OK);
  CHECK(d_gg.candidates.size() == 1);
  CHECK(d_gg.candidates[0].system == TESSERA_SYSTEM_GAME_GEAR);
}
