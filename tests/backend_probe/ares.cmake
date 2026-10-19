set(TESSERA_ARES_SOURCE
    "${PROJECT_SOURCE_DIR}/third_party/backends/ares-0aafd85789215e84e1e43415c07d4c88461b7899/ares-0aafd85789215e84e1e43415c07d4c88461b7899"
    CACHE PATH "Extracted pinned ares v148 source used only by the Gate 0 probe.")
option(TESSERA_ARES_PROFILE_ACCURACY "Use the ares accuracy profile definition in the Gate 0 probe." OFF)

set(ares_sources
    ares/ares/memory/fixed-allocator.cpp
    ares/component/processor/m68000/m68000.cpp
    ares/component/processor/z80/z80.cpp
    ares/component/processor/sh2/sh2.cpp
    ares/component/processor/ssp1601/ssp1601.cpp
    ares/component/audio/sn76489/sn76489.cpp
    ares/component/audio/ym2612/ym2612.cpp
    ares/component/eeprom/m24c/m24c.cpp
    nall/nall/nall.cpp
    nall/nall/sljitAllocator.cpp
    thirdparty/qon/qon.cpp
    thirdparty/sljit/sljit_src/sljitLir.c
    libco/libco.c)
list(TRANSFORM ares_sources PREPEND "${TESSERA_ARES_SOURCE}/")
foreach(source_file IN LISTS ares_sources)
    if(NOT EXISTS "${source_file}")
        message(FATAL_ERROR "ARES_SOURCE_MISSING: ${source_file}. Populate the explicitly pinned cache before configuring.")
    endif()
endforeach()

find_package(Threads REQUIRED)
add_library(tessera_ares_backend STATIC ares_runtime.cpp ${ares_sources})
target_include_directories(tessera_ares_backend SYSTEM PUBLIC
    "${TESSERA_ARES_SOURCE}/ares"
    "${TESSERA_ARES_SOURCE}/nall"
    "${TESSERA_ARES_SOURCE}"
    "${TESSERA_ARES_SOURCE}/thirdparty")
target_compile_definitions(tessera_ares_backend PUBLIC
    CORE_MD SLJIT_HAVE_CONFIG_PRE=1 SLJIT_HAVE_CONFIG_POST=1
    _WIN32_WINNT=0x0A00 _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
    "$<$<CONFIG:Debug>:BUILD_DEBUG>"
    "$<$<NOT:$<CONFIG:Debug>>:BUILD_RELEASE>")
if(TESSERA_ARES_PROFILE_ACCURACY)
    target_compile_definitions(tessera_ares_backend PUBLIC PROFILE_ACCURACY)
else()
    target_compile_definitions(tessera_ares_backend PUBLIC PROFILE_PERFORMANCE)
endif()
target_compile_options(tessera_ares_backend PRIVATE
    /W0 "$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
    "$<$<COMPILE_LANGUAGE:CXX>:/Zc:char8_t->" /utf-8)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(tessera_ares_backend PRIVATE /clang:-fwrapv)
endif()
target_link_libraries(tessera_ares_backend PUBLIC Threads::Threads ws2_32 ole32 shell32 shlwapi advapi32)

add_executable(tessera_ares_probe ares_probe.cpp)
target_link_libraries(tessera_ares_probe PRIVATE Tessera::build_options
    tessera_ares_backend nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)
target_compile_options(tessera_ares_probe PRIVATE /utf-8)
if(TESSERA_ENABLE_DIAGNOSTICS)
    tessera_copy_diagnostic_runtime(tessera_ares_probe)
endif()
