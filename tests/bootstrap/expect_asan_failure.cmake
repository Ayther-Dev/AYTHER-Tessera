if(NOT DEFINED PROBE OR NOT EXISTS "${PROBE}")
    message(FATAL_ERROR "ASAN_PROBE_MISSING: the instrumented negative executable is required.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "ASAN_OPTIONS=abort_on_error=0:halt_on_error=1:exitcode=86" "${PROBE}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    TIMEOUT 10)

if("${probe_result}" STREQUAL "0")
    message(FATAL_ERROR "ASAN_PROBE_NOT_DETECTED: the intentional heap overflow exited successfully.")
endif()
if(NOT probe_error MATCHES "AddressSanitizer: heap-buffer-overflow")
    message(FATAL_ERROR "ASAN_PROBE_WRONG_FAILURE: expected a heap-buffer-overflow diagnostic; result=${probe_result}\n${probe_output}\n${probe_error}")
endif()
message(STATUS "ASan rejected the intentional heap overflow with the required diagnostic.")
