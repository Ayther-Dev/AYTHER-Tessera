// Contrato de observación (spec 008, RF-3.0, TP-006.2): cada constante de Tessera
// coincide con la del header de referencia de gpgx (ayther-abi-1.10-r4).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstdint>
#include <doctest/doctest.h>

#include "tessera/tessera_observation.h"

#ifdef TESSERA_ABI_REFERENCE_HEADER
#include TESSERA_ABI_REFERENCE_HEADER
#include "observation_abi_constants.inc"
#endif

TEST_CASE("la ABI de observación anuncia 1.10") {
    CHECK(TESSERA_OBS_ABI_VERSION_1_10 == 0x0001000Au);
    CHECK(TESSERA_OBS_REGION_COUNT == 27);
    CHECK(TESSERA_OBS_REGION_VRAM == 1);
    CHECK(TESSERA_OBS_REGION_Z80_RAM == 26);
}

TEST_CASE("las capacidades ocupan los bits 0 a 19") {
    CHECK(TESSERA_OBS_CAP_LEGACY_MEMORY == (UINT64_C(1) << 0));
    CHECK(TESSERA_OBS_CAP_SPRITE_OUTCOME_V1 == (UINT64_C(1) << 19));
    CHECK(TESSERA_OBS_CAP_MODE4_CONTROLS == (UINT64_C(1) << 16));
}

TEST_CASE("límites del contrato") {
    CHECK(TESSERA_OBS_JOURNAL_MAX_EVENTS == 256);
    CHECK(TESSERA_OBS_PALETTE_ENTRIES == 256);
    CHECK(TESSERA_OBS_SUB_ALL == 0xFFFu);
}
