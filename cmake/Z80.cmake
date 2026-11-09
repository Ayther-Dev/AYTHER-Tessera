include_guard(GLOBAL)

# floooh/chips z80.h (zlib) is the Z80 of the native Mega Drive core (spec 008,
# DA-4, TP-103.1). The header is pinned in cmake/dependencies.lock.json
# (`component_sources`, name chips-z80) and cached under third_party/cache by
# tools/bootstrap-dependencies.ps1. Configure verifies every file against the
# lock and never downloads.
#
# The header holds C99 anonymous unions, which are not C++: it is compiled as C
# in src/md/z80_chips.c and the C++ side only sees the wrapper in z80_chips.h.
function(tessera_add_z80)
    set(lock_path "${PROJECT_SOURCE_DIR}/cmake/dependencies.lock.json")
    file(READ "${lock_path}" lock)
    string(JSON count LENGTH "${lock}" component_sources)
    set(found FALSE)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON name GET "${lock}" component_sources ${index} name)
        if(NOT name STREQUAL "chips-z80")
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
        message(FATAL_ERROR "DEPENDENCY_LOCK_INVALID: chips-z80 is missing from component_sources.")
    endif()

    add_library(tessera_z80_chips OBJECT "${PROJECT_SOURCE_DIR}/src/md/z80_chips.c")
    target_include_directories(tessera_z80_chips PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_include_directories(tessera_z80_chips SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/${include_dir}")
    if(MSVC)
    target_compile_options(tessera_z80_chips PRIVATE /W0)
else()
    target_compile_options(tessera_z80_chips PRIVATE -w)
endif()
    set_target_properties(tessera_z80_chips PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(Tessera::z80_chips ALIAS tessera_z80_chips)
endfunction()
