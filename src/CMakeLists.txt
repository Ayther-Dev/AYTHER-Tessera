include(GNUInstallDirs)

set(TESSERA_CLOWN_SOURCE "" CACHE PATH "Verified pinned ClownMDEmu source root")
if(NOT TESSERA_CLOWN_SOURCE)
    set(TESSERA_CLOWN_SOURCE "${PROJECT_SOURCE_DIR}/third_party/backends/clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d/clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d")
endif()
if(NOT EXISTS "${TESSERA_CLOWN_SOURCE}/source/clownmdemu.c")
    message(FATAL_ERROR "CLOWN_SOURCE_REQUIRED: prepare the pinned source with tools/bootstrap-backends.ps1 before configuring.")
endif()
set(CC_USE_C99_INTEGERS ON CACHE BOOL "Use fixed C99 integers in ClownMDEmu" FORCE)
include("${PROJECT_SOURCE_DIR}/cmake/ClownProvenance.cmake")
tessera_prepare_clown_provenance("${TESSERA_CLOWN_SOURCE}" TESSERA_CLOWN_PRODUCT_SOURCE)
add_subdirectory("${TESSERA_CLOWN_PRODUCT_SOURCE}" "${PROJECT_BINARY_DIR}/clown-product" EXCLUDE_FROM_ALL)

set(core_sources api/native_api.cpp backends/synthetic/synthetic_backend.cpp session/session.cpp
    session/presentation_transaction.cpp session/session_restore.cpp
    persistence/state_format.cpp persistence/state_store.cpp
    observation/snapshot_store.cpp observation/event_stream.cpp
    systems/system_detector.cpp systems/system_registry.cpp
    backends/megadrive/megadrive_backend.cpp backends/megadrive/megadrive_observation.cpp
    backends/megadrive/canonical_audio.cpp
    backends/megadrive/clown_state_codec.cpp)
add_library(tessera_core STATIC ${core_sources})
add_library(Tessera::core ALIAS tessera_core)
target_include_directories(tessera_core PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(tessera_core SYSTEM PRIVATE "${TESSERA_CLOWN_PRODUCT_SOURCE}/source")
target_link_libraries(tessera_core PRIVATE
    "$<BUILD_INTERFACE:Tessera::build_options>" "$<BUILD_INTERFACE:PicoSHA2::PicoSHA2>"
    "$<BUILD_INTERFACE:clownmdemu-core>")
target_compile_options(tessera_core PRIVATE /utf-8)

file(GLOB_RECURSE identity_sources CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/api/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/*.*" "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/session/*.*" "${CMAKE_CURRENT_SOURCE_DIR}/observation/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/*.*")
list(APPEND identity_sources "${CMAKE_CURRENT_LIST_FILE}" "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/cmake/ClownProvenance.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/patches/clownmdemu-provenance.patch"
    "${PROJECT_SOURCE_DIR}/cmake/dependencies.lock.json" "${PROJECT_SOURCE_DIR}/cmake/Diagnostics.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/TesseraPackage.cmake" "${PROJECT_SOURCE_DIR}/cmake/TesseraConfig.cmake.in")
list(SORT identity_sources)
set(identity_manifest "C++20;C11;${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION};${CMAKE_CXX_COMPILER};${CMAKE_C_COMPILER_VERSION};${CMAKE_BUILD_TYPE};${CMAKE_MSVC_RUNTIME_LIBRARY};${CMAKE_SYSTEM_NAME};${CMAKE_SYSTEM_VERSION};${CMAKE_SIZEOF_VOID_P};${CMAKE_CXX_FLAGS};${CMAKE_C_FLAGS};${CMAKE_CXX_FLAGS_RELWITHDEBINFO};${CMAKE_CXX_FLAGS_RELEASE};${CMAKE_CXX_FLAGS_DEBUG};${CMAKE_C_FLAGS_RELWITHDEBINFO};${CMAKE_C_FLAGS_RELEASE};${CMAKE_C_FLAGS_DEBUG};diagnostics=${TESSERA_ENABLE_DIAGNOSTICS};$ENV{VCToolsVersion};$ENV{WindowsSDKVersion};/W4;/permissive-;/Zc:__cplusplus;/utf-8")
foreach(source IN LISTS identity_sources)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(SHA256 "${source}" source_hash)
    file(RELATIVE_PATH name "${PROJECT_SOURCE_DIR}" "${source}")
    string(APPEND identity_manifest ";${name}=${source_hash}")
endforeach()
string(SHA256 build_id "${identity_manifest}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/build_identity.hpp"
    CONTENT "#pragma once\n#define TESSERA_CORE_BUILD_ID \"@build_id@\"\n" @ONLY)

add_executable(tessera_cli cli/main.cpp cli/replay_trace.cpp)
set_target_properties(tessera_cli PROPERTIES OUTPUT_NAME tessera)
target_link_libraries(tessera_cli PRIVATE Tessera::core Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
target_compile_options(tessera_cli PRIVATE /utf-8)
target_compile_definitions(tessera_cli PRIVATE NOMINMAX)
tessera_copy_diagnostic_runtime(tessera_cli)

include("${PROJECT_SOURCE_DIR}/cmake/TesseraPackage.cmake")
