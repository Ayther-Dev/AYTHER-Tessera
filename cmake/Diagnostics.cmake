include_guard(GLOBAL)

function(tessera_configure_asan_runtime target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MSVC STL's optional/string/vector annotations require the separately installed stl_asan.lib.
        # Disable all STL annotations consistently; compiler ASan address checks remain enabled.
        # The umbrella macro is documented in __msvc_sanitizer_annotate_container.hpp.
        target_compile_definitions(${target} INTERFACE "$<$<COMPILE_LANGUAGE:CXX>:_DISABLE_STL_ANNOTATION>")
        message(STATUS "ASan address instrumentation enabled; MSVC STL container annotations disabled (_DISABLE_STL_ANNOTATION).")
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
            OUTPUT_VARIABLE resource_directory
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE resource_result)
        if(NOT resource_result EQUAL 0)
            message(FATAL_ERROR "ASAN_RESOURCE_QUERY_FAILED: clang-cl could not locate its runtime resources.")
        endif()
        file(TO_CMAKE_PATH "${resource_directory}" resource_directory)
        set(runtime_directories "${resource_directory}/lib/windows" "${resource_directory}/lib/x86_64-pc-windows-msvc")
        find_file(asan_import_library NAMES clang_rt.asan_dynamic-x86_64.lib PATHS ${runtime_directories} NO_DEFAULT_PATH NO_CACHE REQUIRED)
        find_file(asan_runtime_thunk NAMES clang_rt.asan_dynamic_runtime_thunk-x86_64.lib PATHS ${runtime_directories} NO_DEFAULT_PATH NO_CACHE REQUIRED)
        find_file(asan_runtime_dll NAMES clang_rt.asan_dynamic-x86_64.dll PATHS ${runtime_directories} NO_DEFAULT_PATH NO_CACHE REQUIRED)
        # CMake invokes the COFF linker directly, bypassing clang-cl's ASan driver setup.
        target_link_libraries(${target} INTERFACE "${asan_import_library}")
        target_link_options(${target} INTERFACE /include:__asan_seh_interceptor "/wholearchive:${asan_runtime_thunk}")
    else()
        # MSVC selects the ASan libraries from the instrumented object directives.
        get_filename_component(compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_file(asan_runtime_dll NAMES clang_rt.asan_dynamic-x86_64.dll
            PATHS "${compiler_directory}" "$ENV{VCToolsInstallDir}/bin/Hostx64/x64"
            NO_DEFAULT_PATH NO_CACHE REQUIRED)
    endif()
    set(TESSERA_ASAN_RUNTIME_DLL "${asan_runtime_dll}" PARENT_SCOPE)
endfunction()

function(tessera_copy_diagnostic_runtime target)
    if(TESSERA_ENABLE_DIAGNOSTICS AND TESSERA_ASAN_RUNTIME_DLL)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${TESSERA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()
