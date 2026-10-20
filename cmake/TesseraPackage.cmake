include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(TESSERA_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/tessera")
set(TESSERA_INSTALL_ASANDIR "${CMAKE_INSTALL_LIBDIR}/tessera-asan")
set(tessera_package_directory "${CMAKE_CURRENT_BINARY_DIR}/package")
file(MAKE_DIRECTORY "${tessera_package_directory}")
find_package(Git QUIET)
set(tessera_source_revision "unverified")
set(tessera_source_clean_json false)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE tessera_git_revision OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE tessera_git_revision_result ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" status --porcelain --untracked-files=no
        OUTPUT_VARIABLE tessera_git_status OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE tessera_git_status_result ERROR_QUIET)
    string(LENGTH "${tessera_git_revision}" tessera_git_revision_length)
    if(tessera_git_revision_result EQUAL 0 AND
       tessera_git_revision_length EQUAL 40 AND
       tessera_git_revision MATCHES "^[0-9a-f]+$")
        set(tessera_source_revision "${tessera_git_revision}")
    endif()
    if(tessera_git_status_result EQUAL 0 AND tessera_git_status STREQUAL "")
        set(tessera_source_clean_json true)
    endif()
endif()
file(CONFIGURE OUTPUT "${tessera_package_directory}/SOURCE_AVAILABILITY.md" CONTENT [=[# Código fuente correspondiente de Tessera

Este paquete se configuró desde el commit `@tessera_source_revision@` de
https://github.com/Ayther-Dev/AYTHER-Tessera. El árbol de fuentes estaba limpio
al configurarlo: `@tessera_source_clean_json@`. Para una entrega con tag,
obtenga las fuentes de ese tag y compruebe que apunta al commit indicado.

El repositorio contiene los scripts CMake de compilación e instalación, el lock
de fuentes del backend Mega Drive en `third_party/backends/sources.lock.json`,
la descarga y verificación de hashes en `tools/bootstrap-backends.ps1` y el
parche exacto de ClownMDEmu en
`cmake/patches/clownmdemu-provenance.patch`. El lock identifica por separado
las revisiones de ClownMDEmu, clown68000, clownz80 y clowncommon. Sus textos de
licencia originales acompañan al paquete bajo `licenses/`.

Engine es una dependencia separada y aporta sus propias fuentes y avisos.
Esta distribución no incluye ROM, pack de contenido, registro de confianza
ni generador compilado de fixtures.
]=] @ONLY)
install(FILES "${tessera_package_directory}/SOURCE_AVAILABILITY.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/tessera")
set_target_properties(tessera_core PROPERTIES EXPORT_NAME core)
install(TARGETS tessera_core EXPORT TesseraTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
set(TESSERA_INSTALL_BACKEND_LIBDIR "${CMAKE_INSTALL_LIBDIR}/tessera-backend")
set(tessera_backend_libraries clownmdemu-core clown68000-interpreter
    clown68000-common clownz80-interpreter clownz80-common)
foreach(backend_library IN LISTS tessera_backend_libraries)
    install(FILES "$<TARGET_FILE:${backend_library}>"
        DESTINATION "${TESSERA_INSTALL_BACKEND_LIBDIR}")
endforeach()
install(TARGETS tessera_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${PROJECT_SOURCE_DIR}/include/tessera/tessera.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tessera")

set(tessera_diagnostics_json false)
set(tessera_asan_dll_name "")
set(tessera_asan_import_name "")
set(tessera_asan_thunk_name "")
set(tessera_asan_sha256 "")
if(TESSERA_ENABLE_DIAGNOSTICS)
    if(NOT TESSERA_ASAN_RUNTIME_DLL OR NOT EXISTS "${TESSERA_ASAN_RUNTIME_DLL}")
        message(FATAL_ERROR "PACKAGE_ASAN_RUNTIME_MISSING: falta el runtime de diagnóstico local.")
    endif()
    set(tessera_diagnostics_json true)
    get_filename_component(tessera_asan_dll_name "${TESSERA_ASAN_RUNTIME_DLL}" NAME)
    get_filename_component(tessera_asan_directory "${TESSERA_ASAN_RUNTIME_DLL}" DIRECTORY)
    file(SHA256 "${TESSERA_ASAN_RUNTIME_DLL}" tessera_asan_sha256)
    install(FILES "${TESSERA_ASAN_RUNTIME_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(tessera_asan_import_name clang_rt.asan_dynamic-x86_64.lib)
        set(tessera_asan_thunk_name clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
        foreach(runtime_library IN ITEMS "${tessera_asan_import_name}" "${tessera_asan_thunk_name}")
            if(NOT EXISTS "${tessera_asan_directory}/${runtime_library}")
                message(FATAL_ERROR "PACKAGE_ASAN_LIBRARY_MISSING: ${runtime_library}")
            endif()
            install(FILES "${tessera_asan_directory}/${runtime_library}"
                DESTINATION "${TESSERA_INSTALL_ASANDIR}")
        endforeach()
    endif()
endif()

configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/TesseraConfig.cmake.in"
    "${tessera_package_directory}/TesseraConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Tessera"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_BINDIR
        TESSERA_INSTALL_DATADIR TESSERA_INSTALL_ASANDIR
        TESSERA_INSTALL_BACKEND_LIBDIR)
write_basic_package_version_file("${tessera_package_directory}/TesseraConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion)
install(EXPORT TesseraTargets NAMESPACE Tessera::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Tessera")
install(FILES "${tessera_package_directory}/TesseraConfig.cmake"
    "${tessera_package_directory}/TesseraConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Tessera")

# Only the core and CLI closure is installed; probes and fixtures stay private.
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${TESSERA_INSTALL_DATADIR}/licenses")
install(FILES "${PROJECT_SOURCE_DIR}/legal/THIRD_PARTY_NOTICES.md"
    "${PROJECT_SOURCE_DIR}/legal/DEPENDENCIES.csv"
    DESTINATION "${TESSERA_INSTALL_DATADIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/third_party/licenses/picosha2-1.0.1"
    "${PROJECT_SOURCE_DIR}/third_party/licenses/nlohmann-json-3.12.0"
    "${PROJECT_SOURCE_DIR}/third_party/licenses/clownmdemu-core-88ef45a"
    "${PROJECT_SOURCE_DIR}/third_party/licenses/clown68000-bb19838"
    "${PROJECT_SOURCE_DIR}/third_party/licenses/clownz80-aa4cae3"
    "${PROJECT_SOURCE_DIR}/third_party/licenses/clowncommon-ecee31f"
    "${PROJECT_SOURCE_DIR}/third_party/licenses/clowncommon-ddeff17"
    DESTINATION "${TESSERA_INSTALL_DATADIR}/licenses")
install(FILES "${PROJECT_SOURCE_DIR}/cmake/dependencies.lock.json"
    DESTINATION "${TESSERA_INSTALL_DATADIR}")
file(SHA256 "${PROJECT_SOURCE_DIR}/cmake/dependencies.lock.json" tessera_dependency_lock_sha256)
file(CONFIGURE OUTPUT "${tessera_package_directory}/package-metadata.json" CONTENT [=[{
  "schema_version": 1,
  "name": "Tessera",
  "version": "@PROJECT_VERSION@",
  "abi": {"major": 1, "minor": 0, "pointer_bytes": 8, "platform": "windows-x64-msvc", "runtime": "/MD"},
  "build": {"id": "@build_id@", "configuration": "@CMAKE_BUILD_TYPE@", "compiler_id": "@CMAKE_CXX_COMPILER_ID@", "compiler_version": "@CMAKE_CXX_COMPILER_VERSION@", "diagnostics": @tessera_diagnostics_json@},
  "source": {"repository": "https://github.com/Ayther-Dev/AYTHER-Tessera", "revision": "@tessera_source_revision@", "clean_at_configure": @tessera_source_clean_json@},
  "dependencies": {"lock_sha256": "@tessera_dependency_lock_sha256@", "core": ["PicoSHA2 1.0.1", "ClownMDEmu 88ef45a", "clown68000 bb19838", "clownz80 aa4cae3", "clowncommon ecee31f/ddeff17"], "cli": ["PicoSHA2 1.0.1", "nlohmann/json 3.12.0"]},
  "scope": {"core": "synthetic-and-megadrive", "candidate_backends_included": true, "fixtures_included": false, "distribution_review": "T034-pending"},
  "asan_runtime": {"included": @tessera_diagnostics_json@, "file": "@tessera_asan_dll_name@", "sha256": "@tessera_asan_sha256@", "origin": "local-compiler-toolchain", "scope": "local-development-distribution-review-T034"}
}
]=] @ONLY)
install(FILES "${tessera_package_directory}/package-metadata.json"
    DESTINATION "${TESSERA_INSTALL_DATADIR}")
