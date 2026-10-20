add_executable(tessera_backend_probe main.cpp)
target_link_libraries(tessera_backend_probe PRIVATE
    Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
if(TESSERA_ENABLE_DIAGNOSTICS)
    tessera_copy_diagnostic_runtime(tessera_backend_probe)
endif()

option(TESSERA_PROBE_CLOWN "Build the pinned ClownMDEmu evaluation executable." OFF)
option(TESSERA_PROBE_ARES "Build the pinned ares evaluation executable." OFF)
if(TESSERA_PROBE_CLOWN)
    set(TESSERA_CLOWN_SOURCE "" CACHE PATH "Verified Clown source root from bootstrap-backends.ps1.")
    if(NOT EXISTS "${TESSERA_CLOWN_SOURCE}/source/clownmdemu.c")
        message(FATAL_ERROR "CLOWN_SOURCE_REQUIRED: prepare the pinned cache and pass TESSERA_CLOWN_SOURCE.")
    endif()
    set(CC_USE_C99_INTEGERS ON CACHE BOOL "Use fixed C99 integer definitions consistently." FORCE)
    if(NOT TARGET clownmdemu-core)
        add_subdirectory("${TESSERA_CLOWN_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/clown" EXCLUDE_FROM_ALL)
    endif()
    add_executable(tessera_clown_probe clown_probe.cpp)
    target_include_directories(tessera_clown_probe SYSTEM PRIVATE "${TESSERA_CLOWN_SOURCE}/source")
    target_link_libraries(tessera_clown_probe PRIVATE clownmdemu-core
        Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
    set(gate_manifest "clang-cl;C++20;C11;/MD;${CMAKE_CXX_COMPILER_VERSION};${CMAKE_BUILD_TYPE};diagnostics=${TESSERA_ENABLE_DIAGNOSTICS}")
    foreach(source IN ITEMS clown_gate.cpp clown_media.cpp clown_media.hpp clown_media_tests.cpp
            clown_state_codec.cpp clown_state_codec.hpp ../../cmake/dependencies.lock.json
            ../../tools/generate-clown-codec.audit.json CMakeLists.txt)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${source}" source_hash)
        string(APPEND gate_manifest ";${source}=${source_hash}")
    endforeach()
    string(SHA256 gate_build_id "${gate_manifest}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gate_build_id.hpp"
        "#pragma once\n#define TESSERA_GATE_BUILD_ID \"${gate_build_id}\"\n#define TESSERA_ORIGINAL_ROM \"${PROJECT_BINARY_DIR}/fixtures/ayther_synthetic.md\"\n")
    add_executable(tessera_clown_gate_probe clown_gate.cpp clown_media.cpp clown_media_tests.cpp clown_state_codec.cpp)
    target_include_directories(tessera_clown_gate_probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories(tessera_clown_gate_probe SYSTEM PRIVATE "${TESSERA_CLOWN_SOURCE}/source")
    target_link_libraries(tessera_clown_gate_probe PRIVATE clownmdemu-core
        Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
    if(TESSERA_ENABLE_DIAGNOSTICS)
        foreach(target IN ITEMS clownmdemu-core clown68000-interpreter clown68000-common clownz80-interpreter clownz80-common)
            target_link_libraries(${target} PRIVATE Tessera::build_options)
        endforeach()
        tessera_copy_diagnostic_runtime(tessera_clown_probe)
        tessera_copy_diagnostic_runtime(tessera_clown_gate_probe)
    endif()
endif()
if(TESSERA_PROBE_ARES)
    include(ares.cmake)
endif()

if(BUILD_TESTING)
    add_test(NAME phase0.fixture_generation
        COMMAND tessera_backend_probe --generate "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
    set_tests_properties(phase0.fixture_generation PROPERTIES
        LABELS "phase0;fixtures" TIMEOUT 20 FIXTURES_SETUP phase0_probe_fixture)

    add_test(NAME phase0.fixture_reproducibility
        COMMAND "${CMAKE_COMMAND}"
            "-DGENERATOR_EXECUTABLE:FILEPATH=$<TARGET_FILE:tessera_backend_probe>"
            "-DOUTPUT_ROOT:PATH=${CMAKE_CURRENT_BINARY_DIR}/fixture-reproducibility"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixture.cmake")
    set_tests_properties(phase0.fixture_reproducibility PROPERTIES
        LABELS "phase0;fixtures" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results")
    foreach(candidate IN ITEMS clown ares)
        if(TARGET tessera_${candidate}_probe)
            foreach(trace IN ITEMS control action)
                add_test(NAME "phase0.${candidate}_${trace}" COMMAND tessera_${candidate}_probe
                    "${CMAKE_CURRENT_BINARY_DIR}/fixtures" "short-${trace}.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/results/${candidate}-${trace}.json")
                set_tests_properties("phase0.${candidate}_${trace}" PROPERTIES
                    FIXTURES_REQUIRED phase0_probe_fixture RUN_SERIAL TRUE
                    LABELS "phase0;candidate_oracles" TIMEOUT 90)
            endforeach()
        endif()
    endforeach()
    if(TARGET tessera_clown_gate_probe)
        add_test(NAME phase0.clown_gate_contracts COMMAND tessera_clown_gate_probe
            "${CMAKE_CURRENT_BINARY_DIR}/fixtures" short-action.json
            "${CMAKE_CURRENT_BINARY_DIR}/results/clown-gate-short.json" saturated)
        set_tests_properties(phase0.clown_gate_contracts PROPERTIES
            FIXTURES_REQUIRED phase0_probe_fixture RUN_SERIAL TRUE
            LABELS "phase0;qualification" TIMEOUT 180)
    endif()
endif()
