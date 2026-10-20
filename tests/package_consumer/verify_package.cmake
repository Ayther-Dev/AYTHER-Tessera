cmake_minimum_required(VERSION 3.25)
foreach(required PRODUCER_BUILD CONSUMER_SOURCE C_COMPILER CXX_COMPILER GENERATOR CONFIG CTEST_COMMAND)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "PACKAGE_TEST_ARGUMENT_MISSING: ${required}")
    endif()
endforeach()

function(run_checked label)
    list(JOIN ARGN " | " display_command)
    message(STATUS "${label} comando: ${display_command}")
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    message(STATUS "${label} resultado: ${result}\n${output}\n${error}")
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "${label}: ${result}\n${output}\n${error}")
    endif()
endfunction()

# Each invocation owns a new scratch directory and never removes source trees.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef nonce)
file(REAL_PATH "${PRODUCER_BUILD}" producer_absolute)
set(work "${producer_absolute}/package-smoke-${nonce}")
set(original "${work}/prefix-before-move")
set(relocated "${work}/prefix-relocated")
file(MAKE_DIRECTORY "${original}" "${work}/consumer")
file(REAL_PATH "${original}" original_absolute)
cmake_path(IS_PREFIX producer_absolute "${original_absolute}" NORMALIZE original_is_owned)
cmake_path(IS_PREFIX producer_absolute "${relocated}" NORMALIZE relocated_is_owned)
if(NOT original_is_owned OR NOT relocated_is_owned OR
        "${original_absolute}" STREQUAL "${producer_absolute}" OR
        "${relocated}" STREQUAL "${producer_absolute}")
    message(FATAL_ERROR "PACKAGE_SCRATCH_OUTSIDE_BUILD: destino de reubicación fuera del build.")
endif()
run_checked(PACKAGE_INSTALL_FAILED "${CMAKE_COMMAND}" --install "${PRODUCER_BUILD}"
    --config "${CONFIG}" --prefix "${original}")
file(RENAME "${original}" "${relocated}")
file(COPY "${CONSUMER_SOURCE}/CMakeLists.txt" "${CONSUMER_SOURCE}/main.cpp"
    "${CONSUMER_SOURCE}/abi_c.c" DESTINATION "${work}/consumer")
set(common -G "${GENERATOR}" "-DCMAKE_C_COMPILER=${C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}" "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DTESSERA_TEST_PREFIX=${relocated}"
    "-DTESSERA_TEST_SOURCE_PATH=${PROJECT_SOURCE}"
    "-DTESSERA_TEST_BUILD_PATH=${PRODUCER_BUILD}"
    "-DTESSERA_TEST_ORIGINAL_PREFIX=${original}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF)
run_checked(PACKAGE_CONSUMER_CONFIGURE_FAILED "${CMAKE_COMMAND}"
    -S "${work}/consumer" -B "${work}/consumer-build" ${common})
if(NOT EXISTS "${work}/consumer-build/CMakeCache.txt")
    message(FATAL_ERROR "PACKAGE_CONSUMER_CACHE_MISSING: configure no produjo CMakeCache.txt.")
endif()
run_checked(PACKAGE_CONSUMER_BUILD_FAILED "${CMAKE_COMMAND}" --build
    "${work}/consumer-build" --config "${CONFIG}" --parallel 1)
run_checked(PACKAGE_CONSUMER_RUN_FAILED "${CTEST_COMMAND}" --test-dir
    "${work}/consumer-build" -C "${CONFIG}" --output-on-failure)

foreach(negative IN ITEMS version runtime architecture toolset)
    if(negative STREQUAL version)
        set(invalid -DTESSERA_TEST_FIND_VERSION=0.2)
    elseif(negative STREQUAL runtime)
        set(invalid -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded)
    elseif(negative STREQUAL architecture)
        set(invalid -DTESSERA_TEST_BAD_ARCH=ON)
    else()
        set(invalid -DTESSERA_TEST_BAD_TOOLSET=ON)
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${work}/consumer"
        -B "${work}/negative-${negative}" ${common} ${invalid}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "PACKAGE_INCOMPATIBLE_ACCEPTED: ${negative}")
    endif()
    if(negative STREQUAL runtime AND NOT "${output}${error}" MATCHES "TESSERA_RUNTIME_INCOMPATIBLE")
        message(FATAL_ERROR "PACKAGE_RUNTIME_REJECTION_UNIDENTIFIED: ${output}${error}")
    endif()
    if((negative STREQUAL architecture OR negative STREQUAL toolset) AND
            NOT "${output}${error}" MATCHES "TESSERA_TOOLCHAIN_INCOMPATIBLE")
        message(FATAL_ERROR "PACKAGE_TOOLCHAIN_REJECTION_UNIDENTIFIED: ${output}${error}")
    endif()
endforeach()

foreach(path IN ITEMS include/tessera/tessera.h lib/tessera_core.lib bin/tessera.exe
        lib/tessera-backend/clownmdemu-core.lib
        lib/tessera-backend/clown68000-interpreter.lib
        lib/tessera-backend/clown68000-common.lib
        lib/tessera-backend/clownz80-interpreter.lib
        lib/tessera-backend/clownz80-common.lib
        share/tessera/package-metadata.json share/tessera/dependencies.lock.json
        share/tessera/SOURCE_AVAILABILITY.md
        share/tessera/THIRD_PARTY_NOTICES.md share/tessera/DEPENDENCIES.csv
        share/tessera/licenses/LICENSE share/tessera/licenses/picosha2-1.0.1/LICENSE
        share/tessera/licenses/nlohmann-json-3.12.0/LICENSE.MIT
        share/tessera/licenses/clownmdemu-core-88ef45a/LICENCE.txt
        share/tessera/licenses/clown68000-bb19838/LICENCE.txt
        share/tessera/licenses/clownz80-aa4cae3/LICENCE.txt
        share/tessera/licenses/clowncommon-ecee31f/licence.txt
        share/tessera/licenses/clowncommon-ddeff17/licence.txt)
    if(NOT EXISTS "${relocated}/${path}")
        message(FATAL_ERROR "PACKAGE_FILE_MISSING: ${path}")
    endif()
endforeach()
file(READ "${relocated}/share/tessera/package-metadata.json" metadata)
string(JSON package_version GET "${metadata}" version)
string(JSON package_abi GET "${metadata}" abi major)
string(JSON source_revision GET "${metadata}" source revision)
string(LENGTH "${source_revision}" source_revision_length)
if(NOT package_version VERSION_EQUAL "0.1.0" OR NOT package_abi EQUAL 1)
    message(FATAL_ERROR "PACKAGE_METADATA_INVALID: identidad publicada incorrecta.")
endif()
if(NOT source_revision_length EQUAL 40 OR
        NOT source_revision MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "PACKAGE_SOURCE_UNVERIFIED: falta revision exacta.")
endif()
file(READ "${relocated}/share/tessera/SOURCE_AVAILABILITY.md" source_offer)
if(NOT source_offer MATCHES "${source_revision}" OR
        NOT source_offer MATCHES "third_party/backends/sources.lock.json" OR
        NOT source_offer MATCHES "cmake/patches/clownmdemu-provenance.patch")
    message(FATAL_ERROR "PACKAGE_SOURCE_OFFER_INCOMPLETE: fuente o parche no identificados.")
endif()
run_checked(PACKAGE_CLI_FAILED "${relocated}/bin/tessera.exe" backend list)
message(STATUS "Paquete reubicado consumido sin dependencias privadas; negativos de versión, CRT, arquitectura y toolset rechazados.")
