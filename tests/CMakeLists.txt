add_executable(tessera_native_api_test contract/native_api_test.cpp contract/abi_c_test.c)
target_include_directories(tessera_native_api_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(tessera_native_api_test PRIVATE Tessera::build_options doctest::doctest)
if(TARGET Tessera::core)
    target_link_libraries(tessera_native_api_test PRIVATE Tessera::core)
endif()
tessera_copy_diagnostic_runtime(tessera_native_api_test)
add_test(NAME contract.native_api COMMAND tessera_native_api_test)
set_tests_properties(contract.native_api PROPERTIES LABELS "contract;phase1" TIMEOUT 30)

add_executable(tessera_synthetic_test unit/synthetic_backend_test.cpp)
target_include_directories(tessera_synthetic_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_synthetic_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_synthetic_test)
add_test(NAME unit.synthetic_backend COMMAND tessera_synthetic_test)
set_tests_properties(unit.synthetic_backend PROPERTIES LABELS "unit;phase1;FX-01" TIMEOUT 30)

add_executable(tessera_session_test unit/session_test.cpp)
target_include_directories(tessera_session_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_session_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_session_test)
add_test(NAME unit.session COMMAND tessera_session_test)
set_tests_properties(unit.session PROPERTIES LABELS "unit;phase1" TIMEOUT 30)

add_executable(tessera_detector_test unit/system_detector_test.cpp)
target_include_directories(tessera_detector_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_detector_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_detector_test)
add_test(NAME unit.system_detector COMMAND tessera_detector_test)
set_tests_properties(unit.system_detector PROPERTIES LABELS "unit;phase1" TIMEOUT 30)

add_executable(tessera_snapshot_test contract/snapshot_test.cpp)
target_include_directories(tessera_snapshot_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_snapshot_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_snapshot_test)
add_test(NAME contract.snapshot COMMAND tessera_snapshot_test)
set_tests_properties(contract.snapshot PROPERTIES LABELS "contract;phase1" TIMEOUT 60)

add_executable(tessera_cli_test integration/cli_test.cpp)
target_link_libraries(tessera_cli_test PRIVATE Tessera::build_options doctest::doctest nlohmann_json::nlohmann_json)
tessera_copy_diagnostic_runtime(tessera_cli_test)
add_test(NAME integration.cli COMMAND tessera_cli_test "${PROJECT_BINARY_DIR}/src/tessera.exe")
set_tests_properties(integration.cli PROPERTIES LABELS "integration;phase1" TIMEOUT 120)

add_executable(tessera_replay_test integration/replay_test.cpp support/canonical_hash.cpp)
target_include_directories(tessera_replay_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_replay_test PRIVATE Tessera::core Tessera::build_options doctest::doctest nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_replay_test)
add_test(NAME integration.replay COMMAND tessera_replay_test "${PROJECT_BINARY_DIR}/src/tessera.exe" "${PROJECT_BINARY_DIR}/../evidence/phase1-${CMAKE_BUILD_TYPE}-${TESSERA_ENABLE_DIAGNOSTICS}")
set_tests_properties(integration.replay PROPERTIES LABELS "integration;replay;phase1;FX-01" TIMEOUT 240 RUN_SERIAL TRUE)

add_test(NAME package.installed_consumer COMMAND "${CMAKE_COMMAND}"
    "-DPRODUCER_BUILD=${PROJECT_BINARY_DIR}"
    "-DPROJECT_SOURCE=${PROJECT_SOURCE_DIR}"
    "-DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/package_consumer"
    "-DC_COMPILER=${CMAKE_C_COMPILER}"
    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
    "-DGENERATOR=${CMAKE_GENERATOR}" "-DCONFIG=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_consumer/verify_package.cmake")
set_tests_properties(package.installed_consumer PROPERTIES
    LABELS "package;integration;phase2" TIMEOUT 180 RUN_SERIAL TRUE)

set(TESSERA_GENERATED_FIXTURES "${PROJECT_BINARY_DIR}/fixtures")
add_executable(tessera_fixture_roms fixtures/graphics-replacement/generate_roms.cpp)
target_include_directories(tessera_fixture_roms PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/fixtures/ayther-engine-synthetic/upstream/include/ayther")
add_custom_command(OUTPUT
    "${TESSERA_GENERATED_FIXTURES}/ayther_synthetic.md"
    "${TESSERA_GENERATED_FIXTURES}/fx03_rom.md"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TESSERA_GENERATED_FIXTURES}"
    COMMAND "$<TARGET_FILE:tessera_fixture_roms>" "${TESSERA_GENERATED_FIXTURES}"
    DEPENDS tessera_fixture_roms
        "${PROJECT_SOURCE_DIR}/tests/fixtures/graphics-replacement/synth_rom_fx03.h"
    VERBATIM)
add_custom_target(tessera_generate_fixture_roms ALL DEPENDS
    "${TESSERA_GENERATED_FIXTURES}/ayther_synthetic.md"
    "${TESSERA_GENERATED_FIXTURES}/fx03_rom.md")

add_executable(tessera_megadrive_session_test integration/megadrive_session_test.cpp)
target_include_directories(tessera_megadrive_session_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(tessera_megadrive_session_test PRIVATE
    TESSERA_ORIGINAL_ROM="${TESSERA_GENERATED_FIXTURES}/ayther_synthetic.md")
target_link_libraries(tessera_megadrive_session_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_megadrive_session_test)
add_test(NAME integration.megadrive_session COMMAND tessera_megadrive_session_test)
set_tests_properties(integration.megadrive_session PROPERTIES LABELS "integration;phase3;MegaDrive" TIMEOUT 45)

add_executable(tessera_megadrive_observation_test integration/megadrive_observation_test.cpp)
target_include_directories(tessera_megadrive_observation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(tessera_megadrive_observation_test PRIVATE
    TESSERA_ORIGINAL_ROM="${TESSERA_GENERATED_FIXTURES}/ayther_synthetic.md")
target_link_libraries(tessera_megadrive_observation_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_megadrive_observation_test)
add_test(NAME integration.megadrive_observation COMMAND tessera_megadrive_observation_test)
set_tests_properties(integration.megadrive_observation PROPERTIES LABELS "integration;phase4;MegaDrive" TIMEOUT 45)

add_executable(tessera_robustness_test integration/robustness_test.cpp)
target_include_directories(tessera_robustness_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_robustness_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_robustness_test)
add_test(NAME integration.robustness COMMAND tessera_robustness_test)
set_tests_properties(integration.robustness PROPERTIES
    LABELS "integration;phase5;robustness" TIMEOUT 60)

add_executable(tessera_audio_fixture_generator backend_probe/main.cpp)
target_link_libraries(tessera_audio_fixture_generator PRIVATE Tessera::build_options
    nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_audio_fixture_generator)

if(WIN32)
    add_executable(tessera_performance_benchmark performance/benchmark.cpp)
    target_include_directories(tessera_performance_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(tessera_performance_benchmark PRIVATE Tessera::core
        Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
    tessera_copy_diagnostic_runtime(tessera_performance_benchmark)
endif()

add_executable(tessera_state_format_test unit/state_format_test.cpp)
target_include_directories(tessera_state_format_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_state_format_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_state_format_test)
add_test(NAME unit.state_format COMMAND tessera_state_format_test)
set_tests_properties(unit.state_format PROPERTIES LABELS "unit;phase3;checkpoint" TIMEOUT 45)

add_executable(tessera_state_store_test integration/state_store_test.cpp)
target_include_directories(tessera_state_store_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_state_store_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_state_store_test)
add_test(NAME integration.state_store COMMAND tessera_state_store_test)
set_tests_properties(integration.state_store PROPERTIES LABELS "integration;phase3;checkpoint" TIMEOUT 45)

add_executable(tessera_checkpoint_test integration/checkpoint_test.cpp)
target_include_directories(tessera_checkpoint_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_checkpoint_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_checkpoint_test)
add_test(NAME integration.checkpoint COMMAND tessera_checkpoint_test)
set_tests_properties(integration.checkpoint PROPERTIES
    LABELS "integration;phase3;checkpoint;FX-01" TIMEOUT 45)

add_executable(tessera_megadrive_replay_test integration/megadrive_replay_test.cpp)
target_include_directories(tessera_megadrive_replay_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(tessera_megadrive_replay_test PRIVATE
    TESSERA_ORIGINAL_ROM="${TESSERA_GENERATED_FIXTURES}/ayther_synthetic.md"
    TESSERA_FX02_MANIFEST="${PROJECT_SOURCE_DIR}/tests/fixtures/audio-active/manifest.json"
    TESSERA_FX02_GENERATOR_SOURCE="${PROJECT_SOURCE_DIR}/tests/backend_probe/main.cpp")
target_link_libraries(tessera_megadrive_replay_test PRIVATE Tessera::core Tessera::build_options
    nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_megadrive_replay_test)
add_test(NAME integration.megadrive_replay COMMAND tessera_megadrive_replay_test
    "$<TARGET_FILE:tessera_audio_fixture_generator>" "${PROJECT_BINARY_DIR}/../evidence/phase3-${CMAKE_BUILD_TYPE}-${TESSERA_ENABLE_DIAGNOSTICS}")
set_tests_properties(integration.megadrive_replay PROPERTIES
    LABELS "integration;phase3;replay;FX-02;checkpoint" TIMEOUT 900 RUN_SERIAL TRUE)
