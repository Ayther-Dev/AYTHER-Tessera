// Autoprueba de los comparadores del banco (TP-005.6 a TP-005.8, RF-7.2, D-3).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "compare.hpp"

using namespace diffbench;

TEST_CASE("plano 1: detecta un píxel sembrado con frame, línea, x y colores") {
  Frame a{4, 3, std::vector<uint32_t>(12, 0x00112233)};
  Frame b = a;
  CHECK_FALSE(compare_frame(7, a, b).has_value());
  b.pixels[1 * 4 + 2] = 0x00FFFFFF; // línea 1, x 2
  const auto d = compare_frame(7, a, b);
  REQUIRE(d.has_value());
  CHECK(d->frame == 7);
  CHECK(d->line == 1);
  CHECK(d->x == 2);
  CHECK(d->color_a == 0x00112233);
  CHECK(d->color_b == 0x00FFFFFF);
}

TEST_CASE("plano 1: compara color de 24 bits e ignora el byte alto") {
  Frame a{1, 1, {0xFF112233}};
  Frame b{1, 1, {0x00112233}};
  CHECK_FALSE(compare_frame(0, a, b).has_value());
}

TEST_CASE("plano 1: tamaños distintos son una divergencia") {
  Frame a{2, 2, std::vector<uint32_t>(4, 0)};
  Frame b{3, 2, std::vector<uint32_t>(6, 0)};
  CHECK(compare_frame(0, a, b).has_value());
}

TEST_CASE("plano 2: detecta una escritura de audio sembrada") {
  std::vector<AudioWrite> a{{0, 0x22, 0x0F, 100}, {1, 0x28, 0xF0, 200}};
  auto b = a;
  CHECK_FALSE(compare_audio_writes(a, b, false).has_value());
  b[1].data = 0xF1;
  const auto d = compare_audio_writes(a, b, false);
  REQUIRE(d.has_value());
  CHECK(d->index == 1);
}

TEST_CASE("plano 2: el ciclo es opcional") {
  std::vector<AudioWrite> a{{0, 0x22, 0x0F, 100}};
  auto b = a;
  b[0].cycle = 999;
  CHECK_FALSE(compare_audio_writes(a, b, false).has_value());
  CHECK(compare_audio_writes(a, b, true).has_value());
}

TEST_CASE("plano 2: distinta cantidad de escrituras diverge") {
  std::vector<AudioWrite> a{{0, 1, 2, 3}};
  std::vector<AudioWrite> b;
  CHECK(compare_audio_writes(a, b, false).has_value());
}

TEST_CASE("plano 2: RMS y pico del PCM son informativos") {
  const std::vector<int16_t> pcm{3, -4, 0, 0};
  const auto s = pcm_stats(pcm);
  CHECK(s.peak == 4);
  CHECK(s.rms == doctest::Approx(2.5).epsilon(0.001)); // sqrt((9+16)/4)
}

TEST_CASE("regiones: detecta un byte sembrado en VRAM") {
  std::vector<uint8_t> a(64, 0xAA), b = a;
  CHECK_FALSE(compare_region(a, b, {}).has_value());
  b[17] = 0x00;
  const auto d = compare_region(a, b, {});
  REQUIRE(d.has_value());
  CHECK(d->offset == 17);
  CHECK(d->value_a == 0xAA);
  CHECK(d->value_b == 0x00);
}

TEST_CASE("regiones: la máscara oculta bits fuera de comparación") {
  std::vector<uint8_t> a{0b1111'0000}, b{0b1111'0101};
  CHECK(compare_region(a, b, {}).has_value());
  CHECK_FALSE(
      compare_region(a, b, std::vector<uint8_t>{0b1111'0000}).has_value());
}
