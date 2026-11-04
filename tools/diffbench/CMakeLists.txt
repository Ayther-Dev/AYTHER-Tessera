add_executable(tessera_diffbench main.cpp libretro_host.cpp)
target_include_directories(tessera_diffbench PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_diffbench PRIVATE Tessera::core Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)

set(fx "${PROJECT_SOURCE_DIR}/tests/fixtures/diffbench")
add_test(NAME tool.diffbench_help COMMAND tessera_diffbench --help)
set_tests_properties(tool.diffbench_help PROPERTIES
    PASS_REGULAR_EXPRESSION "--oracles.*--corpus.*--entry.*--planes.*--out"
    LABELS "tool;phase0" TIMEOUT 30)
add_test(NAME tool.diffbench_valid_manifest COMMAND tessera_diffbench
    --oracles "${fx}/oracles.toml" --corpus "${fx}/corpus.toml" --validate-only)
set_tests_properties(tool.diffbench_valid_manifest PROPERTIES
    PASS_REGULAR_EXPRESSION "manifiestos v.*lidos" LABELS "tool;phase0" TIMEOUT 30)
add_test(NAME tool.diffbench_invalid_manifest COMMAND tessera_diffbench
    --oracles "${fx}/oracles.toml" --corpus "${fx}/corpus_invalid.toml" --validate-only)
set_tests_properties(tool.diffbench_invalid_manifest PROPERTIES
    PASS_REGULAR_EXPRESSION "sin-hash: sha256" LABELS "tool;phase0" TIMEOUT 30)

set(TESSERA_DIFFBENCH_GPGX_CORE "" CACHE FILEPATH "Core libretro de gpgx (r4) para las pruebas del banco.")
set(TESSERA_DIFFBENCH_FX02_ROM "" CACHE FILEPATH "ROM FX-02 generada por el probe del Gate 0.")
if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_gpgx_fx02 COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --frames 600 --hashes-out "${CMAKE_CURRENT_BINARY_DIR}/gpgx-fx02.hashes")
    set_tests_properties(tool.diffbench_gpgx_fx02 PROPERTIES
        PASS_REGULAR_EXPRESSION "frames=600 hashed=600" LABELS "tool;phase0;local" TIMEOUT 120)
endif()
if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_gpgx_regions COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --frames 60 --dump-regions)
    set_tests_properties(tool.diffbench_gpgx_regions PROPERTIES
        PASS_REGULAR_EXPRESSION "regions=26 readable=26 read_ok=26 size_ok=26" LABELS "tool;phase0;local" TIMEOUT 120)
endif()
if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_trace_b_pressed COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --trace "${fx}/fx02-b-pressed.json" --frames 120)
    set_tests_properties(tool.diffbench_trace_b_pressed PROPERTIES
        PASS_REGULAR_EXPRESSION "audio_peak=4225 " LABELS "tool;phase0;local" TIMEOUT 120)
    add_test(NAME tool.diffbench_trace_neutral COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --trace "${fx}/fx02-neutral.json" --frames 120)
    set_tests_properties(tool.diffbench_trace_neutral PROPERTIES
        PASS_REGULAR_EXPRESSION "audio_peak=768 " LABELS "tool;phase0;local" TIMEOUT 120)
endif()
