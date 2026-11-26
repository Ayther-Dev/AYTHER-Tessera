// API 1.1 de observación (spec 008, RF-3.0, TP-006.4): las firmas son las de
// ayther_interface_v1 con el session como primer parámetro y el contexto al
// final.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <type_traits>

#include "tessera/tessera.h"

using S = tessera_session;
using C = tessera_context_v1;
using R = tessera_status;

#define SIG(fn, ...)                                                           \
  static_assert(                                                               \
      std::is_same_v<decltype(&fn), R(TESSERA_CALL *)(S, __VA_ARGS__, C *)>,   \
      #fn)

SIG(tessera_observation_region_query_v1, uint32_t, tessera_obs_region_info_v1 *,
    uint32_t);
SIG(tessera_observation_region_read_v1, uint32_t, uint32_t, void *, uint32_t,
    uint64_t, uint64_t *);
SIG(tessera_observation_write_control_v1, uint32_t, uint32_t, const void *,
    uint32_t, uint64_t, uint64_t *);
SIG(tessera_observation_snapshot_capture_v1, tessera_obs_frame_snapshot_v1 *,
    uint32_t);
SIG(tessera_observation_recompose_v1, uint16_t *, uint32_t, uint32_t,
    uint32_t *, uint32_t *);
SIG(tessera_observation_recompose_multilayer_v1, uint16_t *, uint16_t *,
    uint16_t *, uint16_t *, uint16_t *, uint32_t, uint32_t, uint32_t *,
    uint32_t *);
SIG(tessera_observation_recompose_stats_v1, tessera_obs_recompose_stats_v1 *,
    uint32_t);
SIG(tessera_observation_audio_events_poll_v1, void *, uint32_t, uint32_t,
    uint32_t *);
SIG(tessera_observation_audio_transport_stats_v1,
    tessera_obs_audio_transport_stats_v1 *, uint32_t);
SIG(tessera_observation_subscriptions_get_v1,
    tessera_obs_subscription_state_v1 *, uint32_t);
SIG(tessera_observation_subscriptions_set_v1, uint32_t);
SIG(tessera_observation_frame_delta_poll_v1, tessera_obs_frame_delta_v1 *,
    uint32_t);
SIG(tessera_observation_frame_delta_since_v1, uint64_t,
    tessera_obs_frame_delta_v1 *, uint32_t);

static_assert(TESSERA_ABI_MINOR == 1, "la API 1.1 sube el menor");

TEST_CASE("la API 1.1 declara las 13 funciones de observación") {
  CHECK(TESSERA_ABI_MINOR == 1);
}

TEST_CASE("limites de observación publicados (TP-006.6, RNF-5)") {
  tessera_observation_limits_v1 out{};
  out.header = {static_cast<uint32_t>(sizeof(out)), TESSERA_ABI_MAJOR,
                TESSERA_ABI_MINOR};
  tessera_context_v1 ctx{};
  ctx.header = {static_cast<uint32_t>(sizeof(ctx)), TESSERA_ABI_MAJOR, 0};
  REQUIRE(tessera_observation_limits_query_v1(0, &out, &ctx) == TESSERA_OK);
  CHECK(out.max_rom_bytes == 16u * 1024 * 1024);
  CHECK(out.max_journal_events == 256);
  CHECK(out.max_audio_writes == 8192);
  CHECK(out.max_parsed_sprites == 128);
  CHECK(out.max_state_bytes == 2u * 1024 * 1024);
}

// decltype(&fn) does not need the definition, so the signatures above pass
// even when an export is missing (rc.2 declared snapshot_capture without
// defining it). Taking every address links each one against Tessera::core.
using AnyFn = void (*)();
#define ADDR(fn) reinterpret_cast<AnyFn>(&fn)
static const AnyFn kObservationFunctions[] = {
    ADDR(tessera_observation_region_query_v1),
    ADDR(tessera_observation_region_read_v1),
    ADDR(tessera_observation_write_control_v1),
    ADDR(tessera_observation_snapshot_capture_v1),
    ADDR(tessera_observation_recompose_v1),
    ADDR(tessera_observation_recompose_multilayer_v1),
    ADDR(tessera_observation_recompose_stats_v1),
    ADDR(tessera_observation_audio_events_poll_v1),
    ADDR(tessera_observation_audio_transport_stats_v1),
    ADDR(tessera_observation_subscriptions_get_v1),
    ADDR(tessera_observation_subscriptions_set_v1),
    ADDR(tessera_observation_frame_delta_poll_v1),
    ADDR(tessera_observation_frame_delta_since_v1),
};

TEST_CASE("the 13 observation functions link against tessera_core") {
  CHECK(sizeof(kObservationFunctions) / sizeof(kObservationFunctions[0]) == 13);
  for (const auto fn : kObservationFunctions)
    CHECK(fn != nullptr);
}

TEST_CASE("snapshot_capture runs and refuses a session that does not exist "
          "(TP-302)") {
  tessera_context_v1 ctx{};
  ctx.header = {static_cast<uint32_t>(sizeof(ctx)), TESSERA_ABI_MAJOR, 0};
  tessera_obs_frame_snapshot_v1 snap{};
  snap.struct_size = 0x5A5A5A5A;
  CHECK(tessera_observation_snapshot_capture_v1(0, &snap, sizeof(snap), &ctx) ==
        TESSERA_INVALID_HANDLE);
  CHECK(snap.struct_size == 0x5A5A5A5A); // untouched
  CHECK(sizeof(tessera_obs_frame_snapshot_v1) == 48);
}
