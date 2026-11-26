include_guard(GLOBAL)

# Moira (MIT) is the 68000 of the native Mega Drive core (spec 008, DA-4).
# Its sources are pinned in cmake/dependencies.lock.json (`component_sources`)
# and cached under third_party/cache by tools/bootstrap-dependencies.ps1.
# Configure verifies every file against the lock and never downloads.
#
# Moira is built from a copy in the build tree with our own MoiraConfig.h
# (src/md/moira/MoiraConfig.h): the vendored one is a Musashi-compatibility
# profile, not the one a real 68000 needs.
function(tessera_add_moira)
    set(lock_path "${PROJECT_SOURCE_DIR}/cmake/dependencies.lock.json")
    file(READ "${lock_path}" lock)
    string(JSON count LENGTH "${lock}" component_sources)
    set(found FALSE)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON name GET "${lock}" component_sources ${index} name)
        if(NOT name STREQUAL "Moira")
            continue()
        endif()
        set(found TRUE)
        string(JSON include_dir GET "${lock}" component_sources ${index} include_directory)
        string(JSON file_count LENGTH "${lock}" component_sources ${index} files)
        math(EXPR last_file "${file_count} - 1")
        foreach(file_index RANGE 0 ${last_file})
            string(JSON relative GET "${lock}" component_sources ${index} files ${file_index} path)
            string(JSON expected GET "${lock}" component_sources ${index} files ${file_index} sha256)
            if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${relative}")
                message(FATAL_ERROR "DEPENDENCY_CACHE_MISSING: ${relative}. Run tools/bootstrap-dependencies.ps1 -Download explicitly; configure never downloads.")
            endif()
            file(SHA256 "${PROJECT_SOURCE_DIR}/${relative}" actual)
            if(NOT actual STREQUAL expected)
                message(FATAL_ERROR "DEPENDENCY_HASH_MISMATCH: ${relative}. The cache must match the tracked lock.")
            endif()
        endforeach()
    endforeach()
    if(NOT found)
        message(FATAL_ERROR "DEPENDENCY_LOCK_INVALID: Moira is missing from component_sources.")
    endif()

    set(source_dir "${PROJECT_SOURCE_DIR}/${include_dir}")
    set(work_dir "${CMAKE_BINARY_DIR}/moira")
    file(GLOB moira_files "${source_dir}/*")
    file(COPY ${moira_files} DESTINATION "${work_dir}")
    file(COPY_FILE "${PROJECT_SOURCE_DIR}/src/md/moira/MoiraConfig.h"
         "${work_dir}/MoiraConfig.h" ONLY_IF_DIFFERENT)

    add_library(tessera_moira OBJECT
        "${work_dir}/Moira.cpp" "${work_dir}/MoiraCore68000.cpp"
        "${work_dir}/MoiraCore68010.cpp" "${work_dir}/MoiraCore68020.cpp"
        "${work_dir}/MoiraDebugger.cpp")
    target_include_directories(tessera_moira SYSTEM PUBLIC "${work_dir}")
    target_compile_features(tessera_moira PUBLIC cxx_std_20)
    if(MSVC)
    target_compile_options(tessera_moira PRIVATE /W0 /EHsc)
else()
    target_compile_options(tessera_moira PRIVATE -w)
endif()
    set_target_properties(tessera_moira PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(Tessera::moira ALIAS tessera_moira)
endfunction()
