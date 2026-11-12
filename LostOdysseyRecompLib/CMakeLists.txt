# Recompiled game code: the XenonRecomp output in ppc/ built as one static library.
# Regenerate ppc/ with: python tools/ppc_codegen.py generate
# (ppc/ is gitignored; run tools/build_tools.bat first)
project(LostOdysseyRecompLib LANGUAGES CXX)

file(GLOB LO_PPC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp.*.cpp")
if(NOT LO_PPC_SOURCES OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")
    message(FATAL_ERROR "No recompiled sources in LostOdysseyRecompLib/ppc; run XenonRecomp first")
endif()
list(APPEND LO_PPC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")

add_library(LostOdysseyRecompLib STATIC ${LO_PPC_SOURCES})
target_include_directories(LostOdysseyRecompLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc"
    "${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde")
target_compile_features(LostOdysseyRecompLib PUBLIC cxx_std_20)

if(LO_TARGET_PLATFORM STREQUAL "switch" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # devkitA64 GCC. ppc_context.h maps the Clang builtins XenonRecomp emits
    # (tools/patches/XenonRecomp-switch.patch). The guest never sees host errno
    # or FP exception flags; rounding stays honoured (-frounding-math). The
    # recompiled code gets x29 back as a general register. Same flags as
    # UnleashedRecomp-NX.
    target_compile_options(LostOdysseyRecompLib PRIVATE
        -frounding-math -fno-math-errno -fno-trapping-math -fomit-frame-pointer)
    if(LO_SWITCH_RECOMP_O2)
        target_compile_options(LostOdysseyRecompLib PRIVATE -O2)
    endif()
    # FP contraction stays at GCC's default (-ffp-contract=fast). PowerPC fmadd/fmadds
    # are fused, and XenonRecomp writes each one as one expression (a * b + c), so
    # contraction keeps them fused; -ffp-contract=off would split about 32,000 of them
    # into two roundings. Only separate double fmul + fadd pairs fuse beyond that, and
    # they are few (about 1,000 fmul); fmuls + fadds round to float in between, which
    # blocks fusion. #17 ruled it out as that bug's cause. See #4.
    # LO_SWITCH_FP_CONTRACT_OFF is for A/B tests: -ffp-contract=on (GCC 15) still fuses
    # within one expression, so fmadd stays fused and only those pairs split.
    if(LO_SWITCH_FP_CONTRACT_OFF)
        target_compile_options(LostOdysseyRecompLib PRIVATE -ffp-contract=on)
    endif()
    # Profile-guided build: only the files that hold the hot code. Instrumenting all 96 MB would make training unusable.
    if(NOT LO_SWITCH_PGO STREQUAL "OFF")
        file(STRINGS "${CMAKE_SOURCE_DIR}/tools/switch/pgo_hot_files.txt" _lo_pgo_names)
        list(TRANSFORM _lo_pgo_names PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/ppc/")
        lo_switch_pgo_sources(${_lo_pgo_names})
    endif()
elseif(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The recompiled code requires clang-cl (uses __int128, weak aliases, builtins)")
endif()
# Same spirit as UnleashedRecompLib: no exceptions, no RTTI, warnings off for generated code.
if(WIN32)
    target_compile_options(LostOdysseyRecompLib PRIVATE /w /EHs-c- /GR- -fno-strict-aliasing
        # MMIO stores (eieio-prefixed) are routed to the runtime's command processor.
        "/FI${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/gpu/ppc_mmio.h")
else()
    target_compile_options(LostOdysseyRecompLib PRIVATE -w -fno-exceptions -fno-rtti -fno-strict-aliasing
        -include "${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/gpu/ppc_mmio.h")
endif()
