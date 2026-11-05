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
        foreach(variant RANGE 0 31)
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
                               "170;176" "180;188" "200;211" "220;223")
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
        # TP-108.4: region from the header and manual selection against gpgx.
        # The probe stores the version register and counts V-ints and main
        # loop passes, so identical work RAM means the same region bits and
        # the same number of lines per frame. Header characters: '_' = space.
        # "2" is Japan PAL, reachable only through the header (gpgx cannot be
        # forced to it).
        foreach(country IN ITEMS U J E UE JE JUE 1 2 4 8 A 0 K _ eur jap usa)
            string(MAKE_C_IDENTIFIER "${country}" country_id)
            add_test(NAME integration.md_region-detect-${country_id}_rom
                COMMAND tessera_md_testrom region-detect-${country}
                "${CMAKE_CURRENT_BINARY_DIR}/region-detect-${country_id}.bin")
            set_tests_properties(integration.md_region-detect-${country_id}_rom PROPERTIES
                FIXTURES_SETUP md_region-detect-${country_id}_rom
                LABELS "integration;phase1;md;local" TIMEOUT 30)
            add_test(NAME integration.md_region-detect-${country_id} COMMAND
                ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/compare_pad.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${CMAKE_CURRENT_BINARY_DIR}/region-detect-${country_id}.bin" 20
                "${CMAKE_CURRENT_BINARY_DIR}/md-region-${country_id}" 0 0 --bytes 4096)
            set_tests_properties(integration.md_region-detect-${country_id} PROPERTIES
                FIXTURES_REQUIRED md_region-detect-${country_id}_rom
                PASS_REGULAR_EXPRESSION "differing=0[
]"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
        # TP-108.5 (risk R-2): pad masks that change from frame to frame are
        # read in the same V-int row by both machines (gpgx samples the pads
        # just before the V-int, the first line of its frame).
        add_test(NAME integration.md_pad-frames_rom
            COMMAND tessera_md_testrom pad-frames "${CMAKE_CURRENT_BINARY_DIR}/pad-frames.bin")
        set_tests_properties(integration.md_pad-frames_rom PROPERTIES
            FIXTURES_SETUP md_pad-frames_rom LABELS "integration;phase1;md;local" TIMEOUT 30)
        foreach(type IN ITEMS 3 6)
            add_test(NAME integration.md_pad-frames-t${type} COMMAND
                ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/compare_pad.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${CMAKE_CURRENT_BINARY_DIR}/pad-frames.bin" 40
                "${CMAKE_CURRENT_BINARY_DIR}/md-pad-frames-t${type}" 0 0
                --type ${type} --bytes 4096
                --script 3:80:0,6:ff:1,9:0:2a,12:5a5:3c3,15:fff:fff,18:0:0,21:41:22)
            set_tests_properties(integration.md_pad-frames-t${type} PROPERTIES
                FIXTURES_REQUIRED md_pad-frames_rom
                PASS_REGULAR_EXPRESSION "differing=0[
]"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
        foreach(forced IN ITEMS u e j)
            add_test(NAME integration.md_region-forced-${forced} COMMAND
                ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/compare_pad.py"
                $<TARGET_FILE:tessera_md_run> $<TARGET_FILE:tessera_diffbench>
                "${TESSERA_DIFFBENCH_GPGX_CORE}"
                "${CMAKE_CURRENT_BINARY_DIR}/region-detect-U.bin" 20
                "${CMAKE_CURRENT_BINARY_DIR}/md-region-forced-${forced}" 0 0
                --region ${forced} --bytes 4096)
            set_tests_properties(integration.md_region-forced-${forced} PROPERTIES
                FIXTURES_REQUIRED md_region-detect-U_rom
                PASS_REGULAR_EXPRESSION "differing=0[
]"
                LABELS "integration;phase1;md;local" TIMEOUT 120)
        endforeach()
    endif()
    if(TESSERA_DIFFBENCH_CORPUS_DIR)
        # TP-110.1: savestate of five corpus games, 400 frames in, 120 after.
        set(savestate_roms "")
        foreach(rom IN ITEMS "Golden Axe (World) (Rev A).md" "Sonic The Hedgehog (USA, Europe).md"
                "Streets of Rage 2 (USA).md" "Phantasy Star IV (USA).md" "Gunstar Heroes (USA).md")
            if(EXISTS "${TESSERA_DIFFBENCH_CORPUS_DIR}/${rom}")
                list(APPEND savestate_roms "${TESSERA_DIFFBENCH_CORPUS_DIR}/${rom}")
            endif()
        endforeach()
        if(savestate_roms)
            string(REPLACE ";" "|" savestate_roms "${savestate_roms}")
            add_test(NAME integration.md_savestate_corpus COMMAND tessera_md_savestate_test
                --test-case=*commercial*)
            set_tests_properties(integration.md_savestate_corpus PROPERTIES
                ENVIRONMENT "TESSERA_SAVESTATE_ROMS=${savestate_roms}"
                LABELS "integration;phase1;md;local" TIMEOUT 900)
        endif()
    endif()
    if(TESSERA_DIFFBENCH_CORPUS_DIR)
        # TP-110.3: two processes, 3 000 frames of the corpus games.
        set(determinism_roms "")
        foreach(rom IN ITEMS "Golden Axe (World) (Rev A).md" "Sonic The Hedgehog (USA, Europe).md"
                "Streets of Rage 2 (USA).md" "Phantasy Star IV (USA).md" "Gunstar Heroes (USA).md")
            if(EXISTS "${TESSERA_DIFFBENCH_CORPUS_DIR}/${rom}")
                list(APPEND determinism_roms "${TESSERA_DIFFBENCH_CORPUS_DIR}/${rom}")
            endif()
        endforeach()
        if(determinism_roms)
            string(REPLACE ";" "|" determinism_roms "${determinism_roms}")
            add_test(NAME integration.md_determinism_corpus COMMAND tessera_md_determinism_test
                --test-case=*commercial*)
            set_tests_properties(integration.md_determinism_corpus PROPERTIES
                ENVIRONMENT "TESSERA_DETERMINISM_ROMS=${determinism_roms}"
                LABELS "integration;phase1;md;local" TIMEOUT 1800)
        endif()
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
    endif()
endif()
