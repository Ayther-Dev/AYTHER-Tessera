add_executable(tessera_dependency_test dependency_test.cpp)
target_link_libraries(tessera_dependency_test PRIVATE Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2 doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_dependency_test)
add_test(NAME bootstrap.dependencies COMMAND tessera_dependency_test)
set_tests_properties(bootstrap.dependencies PROPERTIES LABELS "bootstrap;dependencies" TIMEOUT 10)

add_executable(tessera_c11_test c11_test.c)
target_link_libraries(tessera_c11_test PRIVATE Tessera::build_options)
tessera_copy_diagnostic_runtime(tessera_c11_test)
add_test(NAME bootstrap.c11 COMMAND tessera_c11_test)
set_tests_properties(bootstrap.c11 PROPERTIES LABELS "bootstrap;abi" TIMEOUT 5)

if(TESSERA_ENABLE_DIAGNOSTICS)
    add_executable(tessera_asan_negative asan_negative.cpp)
    target_link_libraries(tessera_asan_negative PRIVATE Tessera::build_options)
    tessera_copy_diagnostic_runtime(tessera_asan_negative)
    add_test(NAME bootstrap.asan_negative
        COMMAND "${CMAKE_COMMAND}" "-DPROBE=$<TARGET_FILE:tessera_asan_negative>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_asan_failure.cmake")
    set_tests_properties(bootstrap.asan_negative PROPERTIES LABELS "bootstrap;diagnostics" TIMEOUT 20)
endif()

if(TESSERA_ENGINE_INTEGRATION)
    set(TESSERA_ENGINE_PREFIX "" CACHE PATH "Installed Engine SDK prefix supplied by the caller.")
    set(TESSERA_ENGINE_DEPENDENCY_PREFIX "" CACHE PATH "Installed Engine native dependency prefix supplied by the caller.")
    if(NOT IS_DIRECTORY "${TESSERA_ENGINE_PREFIX}" OR NOT IS_DIRECTORY "${TESSERA_ENGINE_DEPENDENCY_PREFIX}")
        message(FATAL_ERROR "ENGINE_PREFIX_REQUIRED: set AYTHER_ENGINE_PREFIX and AYTHER_ENGINE_DEPENDENCY_PREFIX to the installed packages.")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${TESSERA_ENGINE_PREFIX}" "${TESSERA_ENGINE_DEPENDENCY_PREFIX}")
    find_package(Ayther 0.1 CONFIG REQUIRED COMPONENTS engine PATHS "${TESSERA_ENGINE_PREFIX}/lib/cmake/Ayther" NO_DEFAULT_PATH)
    add_executable(tessera_engine_link_test engine_link_test.cpp engine_pod.c)
    target_link_libraries(tessera_engine_link_test PRIVATE Tessera::build_options Ayther::engine)
    tessera_copy_diagnostic_runtime(tessera_engine_link_test)
    add_test(NAME bootstrap.engine_link COMMAND tessera_engine_link_test)
    set_tests_properties(bootstrap.engine_link PROPERTIES
        LABELS "bootstrap;engine-link"
        TIMEOUT 5
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${TESSERA_ENGINE_PREFIX}/bin;PATH=path_list_prepend:${TESSERA_ENGINE_DEPENDENCY_PREFIX}/bin")
endif()
