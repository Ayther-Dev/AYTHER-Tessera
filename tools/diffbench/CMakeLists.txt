add_executable(tessera_diffbench main.cpp libretro_host.cpp)
if(TESSERA_BUILD_CLOWN_REFERENCE)
    target_sources(tessera_diffbench PRIVATE tessera_side.cpp)
    target_compile_definitions(tessera_diffbench PRIVATE TESSERA_DIFFBENCH_WITH_TESSERA=1)
endif()
target_include_directories(tessera_diffbench PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tessera_diffbench PRIVATE Tessera::core Tessera::build_options nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2)

set(fx "${PROJECT_SOURCE_DIR}/tests/fixtures/diffbench")
add_test(NAME tool.diffbench_help COMMAND tessera_diffbench --help)
set_tests_properties(tool.diffbench_help PROPERTIES
    PASS_REGULAR_EXPRESSION "--oracles.*--corpus.*--entry.*--planes.*--out"
    LABELS "tool;phase0" TIMEOUT 30)
add_test(NAME tool.diffbench_valid_manifest COMMAND tessera_diffbench
    --oracles "${fx}/oracles.toml" --corpus "${fx}/corpus.toml" --validate-only)
set_tests_properties(tool.diffbench_valid_manifest PROPERTIES
    PASS_REGULAR_EXPRESSION "manifiestos v.*lidos" LABELS "tool;phase0" TIMEOUT 30)
add_test(NAME tool.diffbench_invalid_manifest COMMAND tessera_diffbench
    --oracles "${fx}/oracles.toml" --corpus "${fx}/corpus_invalid.toml" --validate-only)
set_tests_properties(tool.diffbench_invalid_manifest PROPERTIES
    PASS_REGULAR_EXPRESSION "sin-hash: sha256" LABELS "tool;phase0" TIMEOUT 30)

set(TESSERA_DIFFBENCH_GPGX_CORE "" CACHE FILEPATH "Core libretro de gpgx (r4) para las pruebas del banco.")
set(TESSERA_DIFFBENCH_CORPUS_DIR "" CACHE PATH "Carpeta con las ROM del corpus (TP-105.11).")
set(TESSERA_DIFFBENCH_CORPUS_MANIFEST "${PROJECT_SOURCE_DIR}/../specs/008-AYTHER-tessera-parity/tools/corpus.toml" CACHE FILEPATH "Manifiesto del corpus.")
set(TESSERA_DIFFBENCH_FX02_ROM "" CACHE FILEPATH "ROM FX-02 generada por el probe del Gate 0.")
# TP-206.1: the native FRAME_HASH test reads FX-02 where the bench does.
if(TESSERA_DIFFBENCH_FX02_ROM AND TARGET tessera_md_subscriptions_test)
    target_compile_definitions(tessera_md_subscriptions_test PRIVATE
        TESSERA_FX02_ROM="${TESSERA_DIFFBENCH_FX02_ROM}")
endif()
if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_gpgx_fx02 COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --frames 600 --hashes-out "${CMAKE_CURRENT_BINARY_DIR}/gpgx-fx02.hashes")
    set_tests_properties(tool.diffbench_gpgx_fx02 PROPERTIES
        PASS_REGULAR_EXPRESSION "frames=600 hashed=600" LABELS "tool;phase0;local" TIMEOUT 120)
endif()
if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_gpgx_regions COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --frames 60 --dump-regions)
    set_tests_properties(tool.diffbench_gpgx_regions PROPERTIES
        PASS_REGULAR_EXPRESSION "regions=26 readable=26 read_ok=26 size_ok=26" LABELS "tool;phase0;local" TIMEOUT 120)
endif()
if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_trace_b_pressed COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --trace "${fx}/fx02-b-pressed.json" --frames 120)
    set_tests_properties(tool.diffbench_trace_b_pressed PROPERTIES
        PASS_REGULAR_EXPRESSION "audio_peak=4225 " LABELS "tool;phase0;local" TIMEOUT 120)
    add_test(NAME tool.diffbench_trace_neutral COMMAND tessera_diffbench
        --libretro "${TESSERA_DIFFBENCH_GPGX_CORE}" --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --trace "${fx}/fx02-neutral.json" --frames 120)
    set_tests_properties(tool.diffbench_trace_neutral PROPERTIES
        PASS_REGULAR_EXPRESSION "audio_peak=768 " LABELS "tool;phase0;local" TIMEOUT 120)
endif()

if(TESSERA_BUILD_CLOWN_REFERENCE AND TESSERA_DIFFBENCH_FX02_ROM)
    add_test(NAME tool.diffbench_clown_fx02 COMMAND tessera_diffbench
        --tessera clownmdemu --rom "${TESSERA_DIFFBENCH_FX02_ROM}"
        --frames 600 --hashes-out "${CMAKE_CURRENT_BINARY_DIR}/clown-fx02.hashes")
    set_tests_properties(tool.diffbench_clown_fx02 PROPERTIES
        PASS_REGULAR_EXPRESSION "frames=600 hashed=600" LABELS "tool;phase0;local" TIMEOUT 300)
endif()

add_executable(tessera_diffbench_selftest selftest.cpp)
target_link_libraries(tessera_diffbench_selftest PRIVATE Tessera::build_options doctest::doctest)
tessera_copy_diagnostic_runtime(tessera_diffbench_selftest)
add_test(NAME integration.diffbench_selftest COMMAND tessera_diffbench_selftest)
set_tests_properties(integration.diffbench_selftest PROPERTIES LABELS "tool;phase0" TIMEOUT 30)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME tool.diffbench_report_schema COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/test_validate_report.py")
    set_tests_properties(tool.diffbench_report_schema PROPERTIES LABELS "tool;phase0" TIMEOUT 60)
    if(TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
        add_test(NAME tool.diffbench_report_fx02 COMMAND ${CMAKE_COMMAND}
            -DBENCH=$<TARGET_FILE:tessera_diffbench> -DCORE=${TESSERA_DIFFBENCH_GPGX_CORE} -DROM=${TESSERA_DIFFBENCH_FX02_ROM}
            -DPY=${Python3_EXECUTABLE} -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_report.py
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/fx02-report.json -P ${CMAKE_CURRENT_SOURCE_DIR}/report_fx02.cmake)
        set_tests_properties(tool.diffbench_report_fx02 PROPERTIES LABELS "tool;phase0;local" TIMEOUT 120)
        # mode4_pbc: its final frame and PALETTE against gpgx (TP-105.10)
        add_test(NAME integration.md_mode4-pbc_frame COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_frame.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/mode4-pbc.bin" 8
            "${CMAKE_CURRENT_BINARY_DIR}/md-mode4-pbc-frame")
        set_tests_properties(integration.md_mode4-pbc_frame PROPERTIES
            FIXTURES_REQUIRED md_mode4-pbc_rom
            PASS_REGULAR_EXPRESSION "result=identical" LABELS "integration;phase1;md;local" TIMEOUT 120)
        # TP-104.6: every region of mode4_pbc against gpgx. Frame 0 runs in
        # Mode 4 (no sprite or line model there, as for mode4_scene); by
        # frame 8 the ROM is back in Mode 5 and all regions count.
        foreach(case IN ITEMS "1;--no-sprites" "8;")
            list(GET case 0 frames)
            list(GET case 1 extra)
            add_test(NAME integration.md_mode4-pbc_regions-${frames} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_regions.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/mode4-pbc.bin" ${frames}
                "${CMAKE_CURRENT_BINARY_DIR}/md-mode4-pbc-regions-${frames}" ${extra})
            set_tests_properties(integration.md_mode4-pbc_regions-${frames} PROPERTIES
                FIXTURES_REQUIRED md_mode4-pbc_rom
                FAIL_REGULAR_EXPRESSION "differing=[1-9]|different sizes|tool failed"
                PASS_REGULAR_EXPRESSION "WORK_RAM bytes=65536 differing=0"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
    endif()
endif()

if(TESSERA_BUILD_MEGADRIVE)
    add_executable(tessera_md_testrom testrom_main.cpp)
    target_include_directories(tessera_md_testrom PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    target_link_libraries(tessera_md_testrom PRIVATE Tessera::build_options)
    add_executable(tessera_md_run md_run_main.cpp)
    target_include_directories(tessera_md_run PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(tessera_md_run PRIVATE Tessera::core Tessera::build_options)
    tessera_copy_diagnostic_runtime(tessera_md_run)
    # TP-208: cost and memory of the native machine through the public API.
    add_executable(tessera_md_bench md_bench_main.cpp)
    target_include_directories(tessera_md_bench PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(tessera_md_bench PRIVATE Tessera::core Tessera::build_options
        nlohmann_json::nlohmann_json PicoSHA2::PicoSHA2 psapi)
    tessera_copy_diagnostic_runtime(tessera_md_bench)
    if(TESSERA_PERF_BUDGET AND Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE
       AND TESSERA_DIFFBENCH_CORPUS_DIR AND EXISTS "${TESSERA_DIFFBENCH_CORPUS_MANIFEST}")
        # TP-208.1: the provisional G1 targets of plan 2.10 on the corpus. Only
        # in windows-bench, with the power overlay of plan 2.1 set by hand.
        add_test(NAME performance.md_budget COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/perf_budget.py"
            $<TARGET_FILE:tessera_md_bench> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${TESSERA_DIFFBENCH_CORPUS_MANIFEST}"
            "${TESSERA_DIFFBENCH_CORPUS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/md-perf")
        set_tests_properties(performance.md_budget PROPERTIES
            PASS_REGULAR_EXPRESSION "result=ok" RUN_SERIAL TRUE
            LABELS "performance;phase1;md;local" TIMEOUT 1800)
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_FX02_ROM)
        add_test(NAME integration.md_boot_fx02 COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_work_ram.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${TESSERA_DIFFBENCH_FX02_ROM}" 600
            "${CMAKE_CURRENT_BINARY_DIR}/md-boot-fx02")
        set_tests_properties(integration.md_boot_fx02 PROPERTIES
            PASS_REGULAR_EXPRESSION "differing=0" LABELS "integration;phase1;md;local" TIMEOUT 300)
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-104.4: synthetic dma_fifo ROM; VRAM, CRAM and VSRAM are read back
        # into work RAM by the ROM itself, so the work RAM plane covers them.
        add_test(NAME integration.md_dma_fifo_rom COMMAND tessera_md_testrom dma-fifo
            "${CMAKE_CURRENT_BINARY_DIR}/dma_fifo.bin")
        set_tests_properties(integration.md_dma_fifo_rom PROPERTIES
            FIXTURES_SETUP md_dma_fifo_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
        add_test(NAME integration.md_dma_fifo COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_work_ram.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/dma_fifo.bin" 60
            "${CMAKE_CURRENT_BINARY_DIR}/md-dma-fifo")
        set_tests_properties(integration.md_dma_fifo PROPERTIES
            FIXTURES_REQUIRED md_dma_fifo_rom
            PASS_REGULAR_EXPRESSION "differing=0" LABELS "integration;phase1;md;local" TIMEOUT 120)
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-106.1: the list of V counter values seen in the vertical blank.
        foreach(mode IN ITEMS hv-pal-v28 hv-pal-v30)
            add_test(NAME integration.md_${mode}_rom COMMAND tessera_md_testrom ${mode}
                "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin")
            set_tests_properties(integration.md_${mode}_rom PROPERTIES
                FIXTURES_SETUP md_${mode}_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_${mode} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_work_ram.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}" e)
            set_tests_properties(integration.md_${mode} PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "differing=0" LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
        foreach(mode IN ITEMS hv-v28-h32 hv-v28-h40 hv-v30-h32 hv-h32-h hv-h40-h mode4-pbc)
            add_test(NAME integration.md_${mode}_rom COMMAND tessera_md_testrom ${mode}
                "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin")
            set_tests_properties(integration.md_${mode}_rom PROPERTIES
                FIXTURES_SETUP md_${mode}_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_${mode} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_work_ram.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}")
            set_tests_properties(integration.md_${mode} PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "differing=0" LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-106.2: horizontal interrupts per frame in the steady state.
        foreach(mode IN ITEMS hint-counter hint-every-line)
            add_test(NAME integration.md_${mode}_rom COMMAND tessera_md_testrom ${mode}
                "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin")
            set_tests_properties(integration.md_${mode}_rom PROPERTIES
                FIXTURES_SETUP md_${mode}_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_${mode} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_hint_frames.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 30
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}")
            set_tests_properties(integration.md_${mode} PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "match=1" LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-105.2 to TP-105.4: the last frame against gpgx, pixel by pixel.
        # palette_sweep: 9 blocks of 60 colours, normal and shadow (all 512).
        # modes_sweep: scroll modes, plane sizes, window, shadow; the list of
        # variants is modes_variant() in tests/support/md_testrom.hpp.
        set(frame_roms)
        foreach(block RANGE 0 8)
            list(APPEND frame_roms palette-sweep-${block})
        endforeach()
        foreach(variant RANGE 0 33)
            list(APPEND frame_roms modes-sweep-${variant})
        endforeach()
        # sprite_limits (TP-105.5, TP-105.6): sprite table, per line limits,
        # X = 0 masking and the status flags in work RAM. 0 to 25: random
        # and edge scenes; 100 to 120 and 130 to 176: probes of the columns
        # outside the screen that gpgx keeps for collisions; 180 to 188:
        # masking and limits with sprites outside the screen. The list is
        # sprite_variant() in tests/support/md_testrom.hpp.
        set(sprite_variants)
        foreach(range IN ITEMS "0;25" "100;120" "130;135" "140;145" "150;167"
                               "170;176" "180;188" "200;211" "220;225")
            list(GET range 0 first)
            list(GET range 1 last)
            foreach(variant RANGE ${first} ${last})
                list(APPEND sprite_variants ${variant})
            endforeach()
        endforeach()
        # mode4_scene: Master System style VDP through the 68000 (TP-105.10);
        # the list is mode4_variant() in tests/support/md_testrom.hpp.
        foreach(variant RANGE 1 11)
            list(APPEND frame_roms mode4-scene-${variant})
        endforeach()
        foreach(variant IN LISTS sprite_variants)
            list(APPEND frame_roms sprite-limits-${variant})
        endforeach()
        # raster_bands (TP-106.4, TP-201.2, TP-203.1, TP-204.1, TP-204.3):
        # writes to the VDP from the horizontal interrupt, every 8 lines or
        # every line; the list is raster_variant() in
        # tests/support/md_testrom.hpp. Variant 7 (H32
        # and H40 bands) only compares its regions: on its H32 lines gpgx
        # fills columns 256 to 319 with pixels that are not drawn from the
        # scene (open question, see tasks.md TP-106.4).
        foreach(variant RANGE 0 33)
            if(NOT variant EQUAL 7)
                list(APPEND frame_roms raster-bands-${variant})
            endif()
        endforeach()
        add_test(NAME integration.md_raster-bands-7_rom COMMAND tessera_md_testrom
            raster-bands-7 "${CMAKE_CURRENT_BINARY_DIR}/raster-bands-7.bin")
        set_tests_properties(integration.md_raster-bands-7_rom PROPERTIES
            FIXTURES_SETUP md_raster-bands-7_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
        add_test(NAME integration.md_raster-bands-7_regions COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_regions.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/raster-bands-7.bin" 8
            "${CMAKE_CURRENT_BINARY_DIR}/md-raster-bands-7-regions" --no-attribution)
        set_tests_properties(integration.md_raster-bands-7_regions PROPERTIES
            FIXTURES_REQUIRED md_raster-bands-7_rom
            FAIL_REGULAR_EXPRESSION "differing=[1-9]|different sizes|tool failed"
            PASS_REGULAR_EXPRESSION "WORK_RAM bytes=65536 differing=0"
            LABELS "integration;phase1;md;local" TIMEOUT 120)
        foreach(mode IN LISTS frame_roms)
            add_test(NAME integration.md_${mode}_rom COMMAND tessera_md_testrom ${mode}
                "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin")
            set_tests_properties(integration.md_${mode}_rom PROPERTIES
                FIXTURES_SETUP md_${mode}_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_${mode} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_frame.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}")
            set_tests_properties(integration.md_${mode} PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "result=identical" LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
        # TP-201.1, TP-202: the memory regions (VRAM, CRAM, VDP_REGS, VSRAM,
        # Z80_RAM, PARSED_SPRITES and SPRITE_OUTCOME) and the work RAM of the
        # same ROMs, byte for byte.
        foreach(mode IN LISTS frame_roms)
            if(mode MATCHES "^(palette|modes|mode4|sprite-limits|raster-bands)-")
                # gpgx answers PAL to palette-sweep-5 although its header says
                # U (its own choice, not the header rule): the native machine
                # is told so, to compare the PAL SYSTEM too.
                set(region_args)
                if(mode STREQUAL "palette-sweep-5")
                    set(region_args --region e)
                endif()
                if(mode MATCHES "^mode4-")
                    list(APPEND region_args --no-sprites)
                endif()
                add_test(NAME integration.md_${mode}_regions COMMAND ${Python3_EXECUTABLE} -I
                    "${CMAKE_CURRENT_SOURCE_DIR}/compare_regions.py"
                    $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                    "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                    "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}-regions" ${region_args})
                set_tests_properties(integration.md_${mode}_regions PROPERTIES
                    FIXTURES_REQUIRED md_${mode}_rom
                    FAIL_REGULAR_EXPRESSION "differing=[1-9]|different sizes|tool failed"
                    PASS_REGULAR_EXPRESSION "WORK_RAM bytes=65536 differing=0"
                    LABELS "integration;phase1;md;local" TIMEOUT 120)
            endif()
        endforeach()
        # TP-205.1: recompose_frame after the last frame (status, size and
        # pixels), raster_bands 7 included: its recomposition is the frame
        # drawn again from the final state, with no H32 band.
        foreach(mode IN LISTS frame_roms ITEMS raster-bands-7)
            add_test(NAME integration.md_${mode}_recompose COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_recompose.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}-recompose")
            set_tests_properties(integration.md_${mode}_recompose PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "^identical"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
        # TP-205.2: recompose_multilayer with the raster journal replayed
        # (RASTER_TRACKING), every layer alone (RENDER_CONTROLS) or not, and
        # LAYER_MASK / LAYER_DIM written through write_control: the emitted
        # frame, recompose_frame and the five layers. The masks with the
        # window and one plane, the dim over sprites and the hidden Mode 4
        # background are the measured quirks.
        foreach(mode IN LISTS frame_roms)
            foreach(kind IN ITEMS raster isolate)
                set(kind_args --raster)
                if(kind STREQUAL "isolate")
                    list(APPEND kind_args --isolate)
                endif()
                add_test(NAME integration.md_${mode}_multilayer_${kind}
                    COMMAND ${Python3_EXECUTABLE} -I
                    "${CMAKE_CURRENT_SOURCE_DIR}/compare_multilayer.py"
                    $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                    "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                    "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}-multilayer-${kind}" ${kind_args})
                set_tests_properties(integration.md_${mode}_multilayer_${kind} PROPERTIES
                    FIXTURES_REQUIRED md_${mode}_rom
                    PASS_REGULAR_EXPRESSION "^identical"
                    LABELS "integration;phase1;md;local" TIMEOUT 120)
            endforeach()
            if(mode MATCHES "^sprite-limits-")
                set(controls "d:1,a:1")
            else()
                set(controls "5:0,6:0,d:1,a:1")
            endif()
            add_test(NAME integration.md_${mode}_controls COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_controls.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}-controls" "${controls}")
            set_tests_properties(integration.md_${mode}_controls PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                LABELS "integration;phase1;md;local" TIMEOUT 300)
        endforeach()
        # TP-205.3: gpgx still answers the recompose_stats scripts of the
        # fixture as recorded; integration.md_subscriptions runs the same
        # scripts on the native machine.
        add_test(NAME integration.md_recompose_stats_reference COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_recompose_stats.py"
            $<TARGET_FILE:tessera_md_testrom> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}"
            "${PROJECT_SOURCE_DIR}/tests/fixtures/diffbench/recompose-stats.txt"
            "${CMAKE_CURRENT_BINARY_DIR}/md-recompose-stats")
        set_tests_properties(integration.md_recompose_stats_reference PROPERTIES
            PASS_REGULAR_EXPRESSION "^identical: 16 cases"
            LABELS "integration;phase1;md;local" TIMEOUT 300)
        # TP-205.4: frame_delta and frame_delta_since, the same way (the
        # timing fixture holds the cases still open on the native side).
        foreach(fixture IN ITEMS frame-delta:20 frame-delta-timing:5)
            string(REPLACE ":" ";" parts "${fixture}")
            list(GET parts 0 name)
            list(GET parts 1 count)
            add_test(NAME integration.md_${name}_reference COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_recompose_stats.py"
                $<TARGET_FILE:tessera_md_testrom> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${PROJECT_SOURCE_DIR}/tests/fixtures/diffbench/${name}.txt"
                "${CMAKE_CURRENT_BINARY_DIR}/md-${name}")
            set_tests_properties(integration.md_${name}_reference PROPERTIES
                PASS_REGULAR_EXPRESSION "^identical: ${count} cases"
                LABELS "integration;phase1;md;local" TIMEOUT 300)
        endforeach()
        # TP-206.1: FRAME_HASH, the same way (the timing fixture holds frame 1
        # of the test ROMs, still open on the native side); FX-02 when the
        # build knows it.
        foreach(fixture IN ITEMS frame-hash:43 frame-hash-timing:38 mode4-controls:11 limits:5)
            string(REPLACE ":" ";" parts "${fixture}")
            list(GET parts 0 name)
            list(GET parts 1 count)
            add_test(NAME integration.md_${name}_reference COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_recompose_stats.py"
                $<TARGET_FILE:tessera_md_testrom> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${PROJECT_SOURCE_DIR}/tests/fixtures/diffbench/${name}.txt"
                "${CMAKE_CURRENT_BINARY_DIR}/md-${name}")
            set_tests_properties(integration.md_${name}_reference PROPERTIES
                PASS_REGULAR_EXPRESSION "^identical: ${count} cases"
                LABELS "integration;phase1;md;local" TIMEOUT 300)
        endforeach()
        if(TESSERA_DIFFBENCH_FX02_ROM)
            add_test(NAME integration.md_frame-hash-fx02_reference COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_recompose_stats.py"
                $<TARGET_FILE:tessera_md_testrom> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${PROJECT_SOURCE_DIR}/tests/fixtures/diffbench/frame-hash-fx02.txt"
                "${CMAKE_CURRENT_BINARY_DIR}/md-frame-hash-fx02"
                --fx02 "${TESSERA_DIFFBENCH_FX02_ROM}")
            set_tests_properties(integration.md_frame-hash-fx02_reference PROPERTIES
                PASS_REGULAR_EXPRESSION "^identical: 1 cases"
                LABELS "integration;phase1;md;local" TIMEOUT 300)
        endif()
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-103.2/103.3/103.4: the Z80 side. Work RAM and the Z80_RAM region
        # (26) against gpgx after the request / reset sequences, the 68000 view
        # of the Z80 area and the bank window.
        foreach(mode IN ITEMS z80-busreq z80-map z80-bank-0 z80-bank-1 z80-bank-2 z80-bank-3 z80-bank-4 ym-busy-0 ym-busy-1 ym-busy-2 ym-busy-3 ym-busy-4 ym-busy-8)
            add_test(NAME integration.md_${mode}_rom COMMAND tessera_md_testrom ${mode}
                "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin")
            set_tests_properties(integration.md_${mode}_rom PROPERTIES
                FIXTURES_SETUP md_${mode}_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_${mode} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_z80.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin" 8
                "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}")
            set_tests_properties(integration.md_${mode} PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "work_ram bytes=65536 differing=0.*z80_ram bytes=8192 differing=0"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
        # TP-103.4: a poke into Z80_RAM through the control write (a data
        # area the payload never reads) leaves both sides identical.
        add_test(NAME integration.md_z80-poke COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_z80.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/z80-bank-0.bin" 8
            "${CMAKE_CURRENT_BINARY_DIR}/md-z80-poke" 1ff8:01020304:6)
        set_tests_properties(integration.md_z80-poke PROPERTIES
            FIXTURES_REQUIRED md_z80-bank-0_rom
            PASS_REGULAR_EXPRESSION "work_ram bytes=65536 differing=0.*z80_ram bytes=8192 differing=0"
            LABELS "integration;phase1;md;local" TIMEOUT 120)
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-107.2/107.3: AUDIO_WRITES (plane 2) of the last frame against gpgx.
        # The "-once" ROMs run their list in frame 0 with no synchronisation
        # (cycles within one 68000 cycle: the refresh stall lands on another
        # instruction); the others repeat it once per frame, locked to the
        # blanking flag, whose poll phase moves the cycles by up to one poll
        # (200 master cycles) but never the sequence. fm-psg-z80 is the Z80
        # side and matches to the cycle.
        foreach(mode IN ITEMS fm-psg-0-once fm-psg-1-once fm-psg-2-once fm-psg-3-once
                fm-psg-0 fm-psg-1 fm-psg-2 fm-psg-3 fm-psg-z80)
            add_test(NAME integration.md_${mode}_rom COMMAND tessera_md_testrom ${mode}
                "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin")
            set_tests_properties(integration.md_${mode}_rom PROPERTIES
                FIXTURES_SETUP md_${mode}_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
            if(mode MATCHES "once$|z80$")
                set(audio_frames 1)
                set(audio_tolerance 7)
            else()
                set(audio_frames 8)
                set(audio_tolerance 200)
            endif()
            if(mode STREQUAL "fm-psg-z80")
                set(audio_tolerance 0)
            endif()
            add_test(NAME integration.md_${mode} COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_audio.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin"
                ${audio_frames} "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}" --tolerance ${audio_tolerance})
            set_tests_properties(integration.md_${mode} PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "differing=0 "
                LABELS "integration;phase1;md;local" TIMEOUT 120)
            # TP-107.6: the same ROMs through the event transport (RAW_WRITE and
            # FRAME records, and the static part of the transport stats).
            add_test(NAME integration.md_${mode}_events COMMAND ${Python3_EXECUTABLE} -I
                "${CMAKE_CURRENT_SOURCE_DIR}/compare_audio_events.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/${mode}.bin"
                ${audio_frames} "${CMAKE_CURRENT_BINARY_DIR}/md-${mode}-events" --tolerance ${audio_tolerance})
            set_tests_properties(integration.md_${mode}_events PROPERTIES
                FIXTURES_REQUIRED md_${mode}_rom
                PASS_REGULAR_EXPRESSION "differing=0 .*transport_stats .* same"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-108.1/108.2: data port of the pads against gpgx (work RAM bytes
        # stored by pad_latch-N). Type 3 is the three-button pad (TH latch);
        # type 6 the six-button one (gpgx device 0x201), with two mask pairs.
        # Variant 2 sweeps the wait between TH edges (0..3200 cycles).
        foreach(variant IN ITEMS 0 1 2 3 4 5 6)
            set(pad_rom_args "")
            set(pad_masks fff a5a)
            if(variant EQUAL 2)
                set(pad_rom_args 0 3200 100)
                set(pad_masks 0 0) # a pressed A or Start hides the ID bits
            endif()
            add_test(NAME integration.md_pad-latch-${variant}_rom
                COMMAND tessera_md_testrom pad-latch-${variant}
                "${CMAKE_CURRENT_BINARY_DIR}/pad-latch-${variant}.bin" ${pad_rom_args})
            set_tests_properties(integration.md_pad-latch-${variant}_rom PROPERTIES
                FIXTURES_SETUP md_pad-latch-${variant}_rom
                LABELS "integration;phase1;md;local" TIMEOUT 30)
            foreach(type IN ITEMS 3 6)
                if(type EQUAL 3 AND variant EQUAL 2)
                    continue()
                endif()
                add_test(NAME integration.md_pad-latch-${variant}-t${type} COMMAND
                    ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/compare_pad.py"
                    $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                    "${TESSERA_DIFFBENCH_GPGX_CORE}"
                    "${CMAKE_CURRENT_BINARY_DIR}/pad-latch-${variant}.bin" 90
                    "${CMAKE_CURRENT_BINARY_DIR}/md-pad-${variant}-t${type}"
                    ${pad_masks} --type ${type})
                set_tests_properties(integration.md_pad-latch-${variant}-t${type} PROPERTIES
                    FIXTURES_REQUIRED md_pad-latch-${variant}_rom
                    PASS_REGULAR_EXPRESSION "differing=0[
]"
                    LABELS "integration;phase1;md;local" TIMEOUT 120)
            endforeach()
        endforeach()
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE)
        # TP-108.4: the console region from the header, against gpgx. The
        # region_detect ROM stores the I/O version register and the lines per
        # frame in work RAM; the header (hex bytes of the country field) is
        # part of the ROM name. gpgx decides its region from the same header.
        # Cases: letters (single, mixed, either case, past the fourth byte),
        # hex digits (all bits, mixed with letters) and nothing recognised.
        foreach(header IN ITEMS 55 45 4a 4a55 4a45 554a 454a 4a5545 6575 20202045
                2020202045 2045 31 32 33 34 38 39 41 42 43 44 3820 2055 3145 4531
                3845 3255 58 20 30)
            add_test(NAME integration.md_region-detect-${header}_rom
                COMMAND tessera_md_testrom region-detect-${header}
                "${CMAKE_CURRENT_BINARY_DIR}/region-detect-${header}.bin")
            set_tests_properties(integration.md_region-detect-${header}_rom PROPERTIES
                FIXTURES_SETUP md_region-detect-${header}_rom
                LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_region-detect-${header} COMMAND
                ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/compare_region_probe.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${CMAKE_CURRENT_BINARY_DIR}/region-detect-${header}.bin" 30
                "${CMAKE_CURRENT_BINARY_DIR}/md-region-detect-${header}")
            set_tests_properties(integration.md_region-detect-${header} PROPERTIES
                FIXTURES_REQUIRED md_region-detect-${header}_rom
                PASS_REGULAR_EXPRESSION "differing=0[
]"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
    endif()
    if(Python3_Interpreter_FOUND AND TESSERA_DIFFBENCH_GPGX_CORE AND TESSERA_DIFFBENCH_CORPUS_DIR
       AND EXISTS "${TESSERA_DIFFBENCH_CORPUS_MANIFEST}")
        # TP-105.11: tramo 1 de los cinco juegos del corpus en el plano 1.
        add_test(NAME integration.md_corpus COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_corpus.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${TESSERA_DIFFBENCH_CORPUS_MANIFEST}"
            "${TESSERA_DIFFBENCH_CORPUS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/md-corpus"
            --planes framebuffer)
        set_tests_properties(integration.md_corpus PROPERTIES
            PASS_REGULAR_EXPRESSION "result=identical"
            LABELS "integration;phase1;md;local" TIMEOUT 1800)
        # TP-202.1/.2: parsed sprites and their outcome in the corpus games.
        add_test(NAME integration.md_corpus_sprites COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_corpus_sprites.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${TESSERA_DIFFBENCH_CORPUS_MANIFEST}"
            "${TESSERA_DIFFBENCH_CORPUS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/md-corpus-sprites")
        set_tests_properties(integration.md_corpus_sprites PROPERTIES
            PASS_REGULAR_EXPRESSION "result=identical"
            LABELS "integration;phase1;md;local" TIMEOUT 1800)
        # TP-108.4: PAL flag and lines per frame of every corpus game.
        file(GLOB tessera_region_corpus_roms "${TESSERA_DIFFBENCH_CORPUS_DIR}/*.md"
            "${TESSERA_DIFFBENCH_CORPUS_DIR}/*.gen" "${TESSERA_DIFFBENCH_CORPUS_DIR}/*.bin")
        add_test(NAME integration.md_region-corpus COMMAND ${Python3_EXECUTABLE} -I
            "${CMAKE_CURRENT_SOURCE_DIR}/compare_region.py"
            $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
            "${TESSERA_DIFFBENCH_GPGX_CORE}" "${CMAKE_CURRENT_BINARY_DIR}/md-region-corpus"
            ${tessera_region_corpus_roms})
        set_tests_properties(integration.md_region-corpus PROPERTIES
            PASS_REGULAR_EXPRESSION "differing=0[
]"
            LABELS "integration;phase1;md;local" TIMEOUT 600)
    endif()
endif()
