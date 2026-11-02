# cmake/LoSwitchPgo.cmake
# Profile-guided optimisation of the Switch build (docs/SWITCH-PGO.md). The recompiled game is machine-translated
# PowerPC: its branches carry no layout information, so GCC lays blocks out and allocates registers from static guesses.
# A profile of the real game on the console tells it which paths run. Port of SwitchWakerHD's cmake/SwitchPgo.cmake.
#
#   LO_SWITCH_PGO=OFF  (default) a normal build.
#   LO_SWITCH_PGO=GEN  a training build, in its own build folder: the hot game files (tools/switch/pgo_hot_files.txt) and the
#               runtime are instrumented, and the debug server gets "pgo dump|reset|status" (os/switch/pgo_switch.cpp),
#               which writes the counters to sdmc:/switch/LostOdysseyRecomp/pgo/*.gcda. Never released.
#   LO_SWITCH_PGO=USE  the same files compiled with that profile (LO_SWITCH_PGO_DIR).
#
# A profile only fits the sources it was made from: GCC compares a checksum of each function's flow graph and ignores
# the profile of a function that changed (-Wno-coverage-mismatch).
set(LO_SWITCH_PGO "OFF" CACHE STRING "Switch: profile-guided optimisation: OFF, GEN (training build) or USE")
set_property(CACHE LO_SWITCH_PGO PROPERTY STRINGS OFF GEN USE)
set(LO_SWITCH_PGO_DIR "${CMAKE_SOURCE_DIR}/out/pgo" CACHE PATH "LO_SWITCH_PGO=USE: the folder with the .gcda files from the console")
set(LO_SWITCH_PGO_USE_FLAGS "" CACHE STRING "LO_SWITCH_PGO=USE: extra compiler flags for the profiled code")

if(NOT LO_SWITCH_PGO MATCHES "^(OFF|GEN|USE)$")
    message(FATAL_ERROR "LO_SWITCH_PGO must be OFF, GEN or USE")
endif()

set(LO_SWITCH_PGO_FLAGS "")
if(NOT LO_SWITCH_PGO STREQUAL "OFF")
    # The object paths, and so the .gcda names, are relative to the build folder: a profile made by one folder is
    # found by another.
    set(_lo_pgo_common "-fprofile-prefix-path=${CMAKE_BINARY_DIR}")
    if(LO_SWITCH_PGO STREQUAL "GEN")
        # -fprofile-update=single: not atomic. Threads may lose a few increments, which does not change a layout.
        # -fno-profile-values: libgcov's value profiling reads tpidr_el0, which Horizon leaves 0: the first indirect
        # call faults. After changing these flags, delete the objects.
        set(LO_SWITCH_PGO_FLAGS ${_lo_pgo_common} -fprofile-generate=/pgo -fprofile-update=single -fno-profile-values)
    else()
        if(NOT IS_DIRECTORY "${LO_SWITCH_PGO_DIR}")
            message(FATAL_ERROR "LO_SWITCH_PGO=USE: ${LO_SWITCH_PGO_DIR} is not a folder (tools/switch/pgo_train.py fetches the profile)")
        endif()
        # -fprofile-use alone turns on unrolling, peeling and tail duplication, which grew SwitchWakerHD's hot code 29%.
        # The hot code here is already far larger than the A57's L2, so those stay off and cold blocks are moved out.
        set(LO_SWITCH_PGO_FLAGS ${_lo_pgo_common} "-fprofile-use=${LO_SWITCH_PGO_DIR}" -fprofile-correction -fprofile-partial-training
            -fno-reorder-functions -fno-profile-reorder-functions -Wno-missing-profile -Wno-coverage-mismatch
            -fno-tracer -fno-unroll-loops -fno-peel-loops -freorder-blocks-and-partition ${LO_SWITCH_PGO_USE_FLAGS})
    endif()
endif()

# Every source of a target, precompiled header included (per-file flags would not match it). Call it from any folder.
function(lo_switch_pgo_target target)
    if(LO_SWITCH_PGO_FLAGS AND TARGET ${target})
        target_compile_options(${target} PRIVATE ${LO_SWITCH_PGO_FLAGS})
    endif()
endfunction()

# The listed files only: call it from the folder of the target that compiles them, since source properties are per folder.
function(lo_switch_pgo_sources)
    if(NOT LO_SWITCH_PGO_FLAGS)
        return()
    endif()
    string(REPLACE ";" " " _text "${LO_SWITCH_PGO_FLAGS}")
    foreach(_file IN LISTS ARGN)
        set_property(SOURCE "${_file}" APPEND_STRING PROPERTY COMPILE_FLAGS " ${_text}")
    endforeach()
    list(LENGTH ARGN _count)
    message(STATUS "LO_SWITCH_PGO=${LO_SWITCH_PGO}: ${_count} game files")
endfunction()
