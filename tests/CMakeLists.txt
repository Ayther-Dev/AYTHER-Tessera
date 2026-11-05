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
target_compile_definitions(tessera_detector_test PRIVATE TESSERA_MEGADRIVE=$<BOOL:${TESSERA_BUILD_MEGADRIVE}>)
tessera_copy_diagnostic_runtime(tessera_detector_test)
add_test(NAME unit.system_detector COMMAND tessera_detector_test)
set_tests_properties(unit.system_detector PROPERTIES LABELS "unit;phase1" TIMEOUT 30)

if(TESSERA_BUILD_MEGADRIVE)
add_executable(tessera_md_machine_test unit/md_machine_test.cpp)
target_include_directories(tessera_md_machine_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_machine_test PRIVATE Tessera::build_options doctest::doctest)
add_test(NAME unit.md_machine COMMAND tessera_md_machine_test)
set_tests_properties(unit.md_machine PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_cart_test unit/md_cart_test.cpp)
target_include_directories(tessera_md_cart_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_cart_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
target_include_directories(tessera_md_cart_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(tessera_md_cart_test PRIVATE SRM_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/md/bus_sweep.srm")
tessera_copy_diagnostic_runtime(tessera_md_cart_test)
add_test(NAME unit.md_cart COMMAND tessera_md_cart_test)
set_tests_properties(unit.md_cart PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_bus_test unit/md_bus_test.cpp)
target_include_directories(tessera_md_bus_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_bus_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
target_include_directories(tessera_md_bus_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
tessera_copy_diagnostic_runtime(tessera_md_bus_test)
add_executable(tessera_md_m68k_test unit/md_m68k_test.cpp)
target_include_directories(tessera_md_m68k_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_m68k_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_m68k_test)
add_test(NAME unit.md_m68k COMMAND tessera_md_m68k_test)
set_tests_properties(unit.md_m68k PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_z80_test unit/md_z80_test.cpp)
target_include_directories(tessera_md_z80_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_z80_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_z80_test)
add_test(NAME unit.md_z80 COMMAND tessera_md_z80_test)
set_tests_properties(unit.md_z80 PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_ym_test unit/md_ym_test.cpp)
target_include_directories(tessera_md_ym_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_ym_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_ym_test)
add_test(NAME unit.md_ym COMMAND tessera_md_ym_test)
set_tests_properties(unit.md_ym PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_psg_test unit/md_psg_test.cpp)
target_include_directories(tessera_md_psg_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_psg_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_psg_test)
add_test(NAME unit.md_psg COMMAND tessera_md_psg_test)
set_tests_properties(unit.md_psg PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_audio_log_test unit/md_audio_log_test.cpp)
target_include_directories(tessera_md_audio_log_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_audio_log_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_audio_log_test)
add_test(NAME unit.md_audio_log COMMAND tessera_md_audio_log_test)
set_tests_properties(unit.md_audio_log PROPERTIES LABELS "unit;phase1;md" TIMEOUT 60)

add_executable(tessera_md_savestate_test integration/md_savestate_test.cpp)
target_include_directories(tessera_md_savestate_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_savestate_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_savestate_test)
add_test(NAME integration.md_savestate COMMAND tessera_md_savestate_test --test-case=*synthetic*,*refused*)
set_tests_properties(integration.md_savestate PROPERTIES LABELS "integration;phase1;md" TIMEOUT 300)

add_executable(tessera_md_determinism_test integration/md_determinism_test.cpp)
target_include_directories(tessera_md_determinism_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_determinism_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_determinism_test)
add_test(NAME integration.md_determinism COMMAND tessera_md_determinism_test --test-case=*synthetic*,*one\ process*)
set_tests_properties(integration.md_determinism PROPERTIES LABELS "integration;phase1;md" TIMEOUT 600)

add_executable(tessera_md_reset_test integration/md_reset_test.cpp)
target_include_directories(tessera_md_reset_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_reset_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_reset_test)
add_test(NAME integration.md_reset COMMAND tessera_md_reset_test)
set_tests_properties(integration.md_reset PROPERTIES LABELS "integration;phase1;md" TIMEOUT 120)

add_executable(tessera_md_malformed_test integration/md_malformed_test.cpp)
target_include_directories(tessera_md_malformed_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_malformed_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_malformed_test)
add_test(NAME integration.md_malformed COMMAND tessera_md_malformed_test)
set_tests_properties(integration.md_malformed PROPERTIES LABELS "integration;phase1;md" TIMEOUT 300)

add_executable(tessera_md_state_restore_test integration/md_state_restore_test.cpp)
target_include_directories(tessera_md_state_restore_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_state_restore_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_md_state_restore_test)
add_test(NAME integration.md_state_restore COMMAND tessera_md_state_restore_test)
set_tests_properties(integration.md_state_restore PROPERTIES LABELS "integration;phase1;md" TIMEOUT 120)

add_executable(tessera_md_input_test unit/md_input_test.cpp)
target_include_directories(tessera_md_input_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_input_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_md_input_test)
add_test(NAME unit.md_input COMMAND tessera_md_input_test)
set_tests_properties(unit.md_input PROPERTIES LABELS "unit;phase1;md" TIMEOUT 60)

add_executable(tessera_md_pad_test unit/md_pad_test.cpp)
target_include_directories(tessera_md_pad_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_pad_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_pad_test)
add_test(NAME unit.md_pad COMMAND tessera_md_pad_test)
set_tests_properties(unit.md_pad PROPERTIES LABELS "unit;phase1;md" TIMEOUT 60)

add_executable(tessera_md_vdp_regs_dma_test unit/md_vdp_regs_dma_test.cpp)
target_include_directories(tessera_md_vdp_regs_dma_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_vdp_regs_dma_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_vdp_regs_dma_test)
add_test(NAME unit.md_vdp_regs_dma COMMAND tessera_md_vdp_regs_dma_test)
set_tests_properties(unit.md_vdp_regs_dma PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_system_test unit/md_system_test.cpp)
target_include_directories(tessera_md_system_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_md_system_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_system_test)
add_test(NAME unit.md_system COMMAND tessera_md_system_test)
set_tests_properties(unit.md_system PROPERTIES LABELS "unit;phase1;md" TIMEOUT 60)

add_executable(tessera_md_testrom_bus_sweep integration/md_testrom_bus_sweep.cpp)
target_include_directories(tessera_md_testrom_bus_sweep PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_md_testrom_bus_sweep PRIVATE Tessera::core Tessera::build_options doctest::doctest)
target_compile_definitions(tessera_md_testrom_bus_sweep PRIVATE
    GOLDEN_FILE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/md/bus_sweep.golden"
    SRM_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/md/bus_sweep.srm")
tessera_copy_diagnostic_runtime(tessera_md_testrom_bus_sweep)
add_test(NAME integration.md_testrom_bus_sweep COMMAND tessera_md_testrom_bus_sweep)
set_tests_properties(integration.md_testrom_bus_sweep PROPERTIES LABELS "integration;phase1;md" TIMEOUT 60)

add_executable(tessera_md_vdp_reads_test unit/md_vdp_reads_test.cpp)
target_include_directories(tessera_md_vdp_reads_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_md_vdp_reads_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_vdp_reads_test)
add_test(NAME unit.md_vdp_reads COMMAND tessera_md_vdp_reads_test)
set_tests_properties(unit.md_vdp_reads PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_vdp_render_test unit/md_vdp_render_test.cpp)
target_include_directories(tessera_md_vdp_render_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_md_vdp_render_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_vdp_render_test)
add_test(NAME unit.md_vdp_render COMMAND tessera_md_vdp_render_test)
set_tests_properties(unit.md_vdp_render PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_test(NAME unit.md_bus COMMAND tessera_md_bus_test)
set_tests_properties(unit.md_bus PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)

add_executable(tessera_md_tmss_test unit/md_tmss_test.cpp)
target_include_directories(tessera_md_tmss_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_md_tmss_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_md_tmss_test)
add_test(NAME unit.md_tmss COMMAND tessera_md_tmss_test)
set_tests_properties(unit.md_tmss PROPERTIES LABELS "unit;phase1;md" TIMEOUT 30)
endif()

add_executable(tessera_snapshot_test contract/snapshot_test.cpp)
target_include_directories(tessera_snapshot_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_snapshot_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_snapshot_test)
add_test(NAME contract.snapshot COMMAND tessera_snapshot_test)
set_tests_properties(contract.snapshot PROPERTIES LABELS "contract;phase1" TIMEOUT 60)

if(WIN32) # the CLI and the process-spawning tests are Windows-only for now
add_executable(tessera_cli_test integration/cli_test.cpp)
target_link_libraries(tessera_cli_test PRIVATE Tessera::build_options doctest::doctest nlohmann_json::nlohmann_json)
target_compile_definitions(tessera_cli_test PRIVATE
    TESSERA_CLOWN_REFERENCE=$<BOOL:${TESSERA_BUILD_CLOWN_REFERENCE}>
    TESSERA_MEGADRIVE=$<BOOL:${TESSERA_BUILD_MEGADRIVE}>)
tessera_copy_diagnostic_runtime(tessera_cli_test)
add_test(NAME integration.cli COMMAND tessera_cli_test "${PROJECT_BINARY_DIR}/src/tessera.exe")
set_tests_properties(integration.cli PROPERTIES LABELS "integration;phase1" TIMEOUT 120)

add_executable(tessera_replay_test integration/replay_test.cpp support/canonical_hash.cpp)
target_include_directories(tessera_replay_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_replay_test PRIVATE Tessera::core Tessera::build_options doctest::doctest nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_replay_test)
add_test(NAME integration.replay COMMAND tessera_replay_test "${PROJECT_BINARY_DIR}/src/tessera.exe" "${PROJECT_BINARY_DIR}/../evidence/phase1-${CMAKE_BUILD_TYPE}-${TESSERA_ENABLE_DIAGNOSTICS}")
set_tests_properties(integration.replay PROPERTIES LABELS "integration;replay;phase1;FX-01" TIMEOUT 240 RUN_SERIAL TRUE)
endif()

# El paquete distribuible solo lista los backends sintéticos; con los núcleos
# de referencia o el núcleo Mega Drive en construcción se verifica en windows-dev.
if(WIN32 AND NOT TESSERA_BUILD_CLOWN_REFERENCE AND NOT TESSERA_BUILD_MEGADRIVE)
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
endif()

# Mega Drive tests need the ClownMDEmu reference backend (AGPL-3.0), which is
# built only in the local reference tree and never distributed.
if(TESSERA_BUILD_CLOWN_REFERENCE)
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
endif()

add_executable(tessera_robustness_test integration/robustness_test.cpp)
target_include_directories(tessera_robustness_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_robustness_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_robustness_test)
add_test(NAME integration.robustness COMMAND tessera_robustness_test)
set_tests_properties(integration.robustness PROPERTIES
    LABELS "integration;phase5;robustness" TIMEOUT 60)

if(TESSERA_BUILD_CLOWN_REFERENCE)
add_executable(tessera_audio_fixture_generator backend_probe/main.cpp)
target_link_libraries(tessera_audio_fixture_generator PRIVATE Tessera::build_options
    nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_audio_fixture_generator)
endif()

if(WIN32 AND TESSERA_BUILD_CLOWN_REFERENCE)
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

if(WIN32)
add_executable(tessera_state_store_test integration/state_store_test.cpp)
target_include_directories(tessera_state_store_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_state_store_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_state_store_test)
add_test(NAME integration.state_store COMMAND tessera_state_store_test)
set_tests_properties(integration.state_store PROPERTIES LABELS "integration;phase3;checkpoint" TIMEOUT 45)
endif()

add_executable(tessera_checkpoint_test integration/checkpoint_test.cpp)
target_include_directories(tessera_checkpoint_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_checkpoint_test PRIVATE Tessera::core Tessera::build_options
    doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_checkpoint_test)
add_test(NAME integration.checkpoint COMMAND tessera_checkpoint_test)
set_tests_properties(integration.checkpoint PROPERTIES
    LABELS "integration;phase3;checkpoint;FX-01" TIMEOUT 45)

if(TESSERA_BUILD_CLOWN_REFERENCE)
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
endif()

add_executable(tessera_observation_abi_test contract/observation_abi_test.cpp)
target_include_directories(tessera_observation_abi_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/contract")
target_link_libraries(tessera_observation_abi_test PRIVATE Tessera::build_options doctest::doctest)
set(TESSERA_ABI_REFERENCE_HEADER "" CACHE FILEPATH "Header ayther_api.h de referencia (gpgx ayther-abi-1.10-r4) para contract.observation_abi.")
if(TESSERA_ABI_REFERENCE_HEADER)
    target_compile_definitions(tessera_observation_abi_test PRIVATE TESSERA_ABI_REFERENCE_HEADER="${TESSERA_ABI_REFERENCE_HEADER}")
endif()
tessera_copy_diagnostic_runtime(tessera_observation_abi_test)
add_test(NAME contract.observation_abi COMMAND tessera_observation_abi_test)
set_tests_properties(contract.observation_abi PROPERTIES LABELS "contract;phase1;observation" TIMEOUT 30)

add_executable(tessera_observation_api_test contract/observation_api_test.cpp)
target_include_directories(tessera_observation_api_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(tessera_observation_api_test PRIVATE Tessera::core Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_observation_api_test)
add_test(NAME contract.observation_api COMMAND tessera_observation_api_test)
set_tests_properties(contract.observation_api PROPERTIES LABELS "contract;phase1;observation" TIMEOUT 30)

add_executable(tessera_synthetic_observation_test contract/synthetic_observation_test.cpp)
target_include_directories(tessera_synthetic_observation_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tessera_synthetic_observation_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_synthetic_observation_test)
add_test(NAME contract.synthetic_observation COMMAND tessera_synthetic_observation_test)
set_tests_properties(contract.synthetic_observation PROPERTIES LABELS "contract;phase1;observation" TIMEOUT 30)

add_executable(tessera_md_mixer_test unit/md_mixer_test.cpp)
target_include_directories(tessera_md_mixer_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_mixer_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_md_mixer_test)
add_test(NAME unit.md_mixer COMMAND tessera_md_mixer_test)
set_tests_properties(unit.md_mixer PROPERTIES LABELS "unit;phase1;md" TIMEOUT 60)

if(TESSERA_BUILD_MEGADRIVE)
add_executable(tessera_md_audio_mute_test unit/md_audio_mute_test.cpp)
target_include_directories(tessera_md_audio_mute_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(tessera_md_audio_mute_test PRIVATE Tessera::core Tessera::build_options doctest::doctest PicoSHA2::PicoSHA2)
tessera_copy_diagnostic_runtime(tessera_md_audio_mute_test)
add_test(NAME unit.md_audio_mute COMMAND tessera_md_audio_mute_test)
set_tests_properties(unit.md_audio_mute PROPERTIES LABELS "unit;phase1;md" TIMEOUT 60)
endif()
