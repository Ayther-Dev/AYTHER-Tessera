add_executable(tessera_diffbench main.cpp libretro_host.cpp)
if(TESSERA_BUILD_CLOWN_REFERENCE)
    target_sources(tessera_diffbench PRIVATE tessera_side.cpp)
    target_compile_definitions(tessera_diffbench PRIVATE TESSERA_DIFFBENCH_WITH_TESSERA=1)
endif()
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

if(TESSERA_BUILD_CLOWN_REFERENCE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_clown_fx02 COMMAND tessera_diffbench
        --tessera clownmdemu --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --frames 600 --hashes-out "${CMAKE_CURRENT_BINARY_DIR}/clown-fx02.hashes")
    set_tests_properties(tool.diffbench_clown_fx02 PROPERTIES
        PASS_REGULAR_EXPRESSION "frames=600 hashed=600" LABELS "tool;phase0;local" TIMEOUT 300)
endif()

add_executable(tessera_diffbench_selftest selftest.cpp)
target_link_libraries(tessera_diffbench_selftest PRIVATE Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_diffbench_selftest)
add_test(NAME integration.diffbench_selftest COMMAND tessera_diffbench_selftest)
set_tests_properties(integration.diffbench_selftest PROPERTIES LABELS "tool;phase0" TIMEOUT 30)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME tool.diffbench_report_schema COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/test_validate_report.py")
    set_tests_properties(tool.diffbench_report_schema PROPERTIES LABELS "tool;phase0" TIMEOUT 60)
    if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
        add_test(NAME tool.diffbench_report_fx02 COMMAND ${CMAKE_COMMAND}
            -DBENCH=$<TARGET_FILE:tessera_diffbench> -DCORE=${TESSERA_DIFFBENCH_GPGX_CORE} -DROM=${TESSERA_DIFFBENCH_FX02_ROM}
            -DPY=${Python3_EXECUTABLE} -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_report.py
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/fx02-report.json -P ${CMAKE_CURRENT_SOURCE_DIR}/report_fx02.cmake)
        set_tests_properties(tool.diffbench_report_fx02 PROPERTIES LABELS "tool;phase0;local" TIMEOUT 120)
    endif()
endif()

if(TESSERA_BUILD_MEGADRIVE)
    add_executable(tessera_md_testrom testrom_main.cpp)
    target_include_directories(tessera_md_testrom PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    target_link_libraries(tessera_md_testrom PRIVATE Tessera::build_options)
endif()
