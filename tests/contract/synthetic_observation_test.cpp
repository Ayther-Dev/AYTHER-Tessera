// synthetic.complete sirve las 26 regiones de la ABI 1.10 con su tamaño y
// rechaza la escritura de supresión (spec 008, DA-2, PQ-1, TP-006.5).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>

#include "support/fx01.hpp"

using namespace fx01;

static const std::uint32_t kSizes[27] = {
    0,    65536, 128, 32,  128,  1,    16,   512,   3072, 1,
    1,    65536, 4,   1280, 1,    4,    4,    71680, 44,   7192,
    152,  59160, 2064, 56, 512,  80,   8192};

TEST_CASE("cada región se consulta y se lee con el tamaño de la ABI") {
  Session s(true);
  for (std::uint32_t id = 1; id <= 26; ++id) {
    tessera_obs_region_info_v1 info{};
    auto ctx = init<tessera_context_v1>();
    REQUIRE(tessera_observation_region_query_v1(s.handle, id, &info,
                                                sizeof(info), &ctx) ==
            TESSERA_OK);
    CHECK(info.region_id == id);
    CHECK(info.byte_size == kSizes[id]);
    std::vector<std::uint8_t> buf(info.byte_size);
    std::uint64_t generation = 0;
    REQUIRE(tessera_observation_region_read_v1(s.handle, id, 0, buf.data(),
                                               info.byte_size, UINT64_MAX,
                                               &generation, &ctx) ==
            TESSERA_OK);
  }
}

TEST_CASE("una región desconocida no existe") {
  Session s(true);
  tessera_obs_region_info_v1 info{};
  auto ctx = init<tessera_context_v1>();
  CHECK(tessera_observation_region_query_v1(s.handle, 27, &info, sizeof(info),
                                            &ctx) == TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("write_control sobre las regiones de supresión 6 a 9 se rechaza") {
  Session s(true);
  for (std::uint32_t id = 6; id <= 9; ++id) {
    std::uint8_t data[1] = {1};
    std::uint64_t generation = 0;
    auto ctx = init<tessera_context_v1>();
    CHECK(tessera_observation_write_control_v1(s.handle, id, 0, data, 1,
                                               UINT64_MAX, &generation,
                                               &ctx) ==
          TESSERA_UNSUPPORTED_CAPABILITY);
  }
}

TEST_CASE("write_control admite LAYER_MASK, LAYER_DIM y AUDIO_MUTE") {
  Session s(true);
  for (std::uint32_t id : {5u, 10u, 15u}) {
    std::uint8_t data[1] = {0};
    std::uint64_t generation = 0;
    auto ctx = init<tessera_context_v1>();
    CHECK(tessera_observation_write_control_v1(s.handle, id, 0, data, 1,
                                               UINT64_MAX, &generation,
                                               &ctx) == TESSERA_OK);
  }
}

TEST_CASE("el sintético mínimo no sirve observación") {
  Session s(false);
  tessera_obs_region_info_v1 info{};
  auto ctx = init<tessera_context_v1>();
  CHECK(tessera_observation_region_query_v1(s.handle, 1, &info, sizeof(info),
                                            &ctx) ==
        TESSERA_UNSUPPORTED_CAPABILITY);
}
