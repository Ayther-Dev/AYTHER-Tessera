// API 1.1 de observación (spec 008, RF-3.0, TP-006.4): las firmas son las de
// ayther_interface_v1 con el session como primer parámetro y el contexto al final.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <type_traits>

#include "tessera/tessera.h"

using S = tessera_session;
using C = tessera_context_v1;
using R = tessera_status;

#define SIG(fn, ...) \
    static_assert(std::is_same_v<decltype(&fn), R(TESSERA_CALL *)(S, __VA_ARGS__, C *)>, #fn)

SIG(tessera_observation_region_query_v1, uint32_t, tessera_obs_region_info_v1 *, uint32_t);
SIG(tessera_observation_region_read_v1, uint32_t, uint32_t, void *, uint32_t, uint64_t, uint64_t *);
SIG(tessera_observation_write_control_v1, uint32_t, uint32_t, const void *, uint32_t, uint64_t, uint64_t *);
SIG(tessera_observation_snapshot_capture_v1, tessera_obs_frame_snapshot_v1 *, uint32_t);
SIG(tessera_observation_recompose_v1, uint16_t *, uint32_t, uint32_t, uint32_t *, uint32_t *);
SIG(tessera_observation_recompose_multilayer_v1, uint16_t *, uint16_t *, uint16_t *, uint16_t *, uint16_t *,
    uint32_t, uint32_t, uint32_t *, uint32_t *);
SIG(tessera_observation_recompose_stats_v1, tessera_obs_recompose_stats_v1 *, uint32_t);
SIG(tessera_observation_audio_events_poll_v1, void *, uint32_t, uint32_t, uint32_t *);
SIG(tessera_observation_audio_transport_stats_v1, tessera_obs_audio_transport_stats_v1 *, uint32_t);
SIG(tessera_observation_subscriptions_get_v1, tessera_obs_subscription_state_v1 *, uint32_t);
SIG(tessera_observation_subscriptions_set_v1, uint32_t);
SIG(tessera_observation_frame_delta_poll_v1, tessera_obs_frame_delta_v1 *, uint32_t);
SIG(tessera_observation_frame_delta_since_v1, uint64_t, tessera_obs_frame_delta_v1 *, uint32_t);

static_assert(TESSERA_ABI_MINOR == 1, "la API 1.1 sube el menor");

TEST_CASE("la API 1.1 declara las 13 funciones de observación") { CHECK(TESSERA_ABI_MINOR == 1); }
