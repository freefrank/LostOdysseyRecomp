# cmake/LoOptimization.cmake
# LORecomp Clang optimization, architecture, LTO, and PGO build configuration.

# Export compile commands for tooling inspection (compile_commands.json)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

# Target CPU architecture
set(LO_TARGET_ARCH "sandybridge" CACHE STRING "Target CPU architecture (e.g. sandybridge, znver2)")

# Optimization level
set(LO_OPT_LEVEL "" CACHE STRING "Explicit optimization level (2, 3, or empty to follow build type)")

# Link-Time Optimization (LTO) mode
set(LO_LTO_MODE "off" CACHE STRING "Link-time optimization mode (off, thin, full)")

# Profile-Guided Optimization (PGO) mode
set(LO_PGO_MODE "off" CACHE STRING "Profile-guided optimization mode (off, generate, use)")
set(LO_PGO_FILE "" CACHE FILEPATH "Absolute path to merged profile data for PGO use phase")

# BOLT preparation: keep relocations
option(LO_BOLT_READY "Emit relocations for post-link BOLT optimization" OFF)

# Diagnostic profile build: retain frame pointers and full debug info
option(LO_PROFILE_DIAGNOSTIC "Retain frame pointers and debug line tables for profiling" OFF)

# ThinLTO cache configuration
set(LO_THINLTO_CACHE_DIR "" CACHE PATH "Directory for ThinLTO cache")
set(LO_THINLTO_JOBS "" CACHE STRING "Concurrency limit for ThinLTO code generation")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Architecture option
    if(LO_TARGET_ARCH)
        add_compile_options(-march=${LO_TARGET_ARCH})
    endif()

    # Explicit optimization level
    if(LO_OPT_LEVEL STREQUAL "2" OR LO_OPT_LEVEL STREQUAL "3")
        add_compile_options(-O${LO_OPT_LEVEL})
    elseif(LO_OPT_LEVEL AND NOT LO_OPT_LEVEL STREQUAL "")
        message(FATAL_ERROR "LO_OPT_LEVEL must be '2', '3', or empty; got '${LO_OPT_LEVEL}'")
    endif()

    # LTO mode
    if(LO_LTO_MODE STREQUAL "thin")
        add_compile_options(-flto=thin)
        add_link_options(-flto=thin)
        if(UNIX AND LO_THINLTO_CACHE_DIR)
            add_link_options("-Wl,--thinlto-cache-dir=${LO_THINLTO_CACHE_DIR}")
        endif()
        if(UNIX AND LO_THINLTO_JOBS)
            add_link_options("-Wl,--thinlto-jobs=${LO_THINLTO_JOBS}")
        endif()
    elseif(LO_LTO_MODE STREQUAL "full")
        add_compile_options(-flto=full)
        add_link_options(-flto=full)
    elseif(NOT LO_LTO_MODE STREQUAL "off")
        message(FATAL_ERROR "LO_LTO_MODE must be 'off', 'thin', or 'full'; got '${LO_LTO_MODE}'")
    endif()

    # PGO mode
    if(LO_PGO_MODE STREQUAL "generate")
        add_compile_options(-fprofile-generate -fprofile-update=atomic)
        add_link_options(-fprofile-generate)
    elseif(LO_PGO_MODE STREQUAL "use")
        if(NOT LO_PGO_FILE)
            message(FATAL_ERROR "LO_PGO_MODE is 'use' but LO_PGO_FILE is not set")
        endif()
        if(NOT EXISTS "${LO_PGO_FILE}")
            message(FATAL_ERROR "LO_PGO_FILE does not exist: '${LO_PGO_FILE}'")
        endif()
        add_compile_options(-fprofile-use=${LO_PGO_FILE})
        add_link_options(-fprofile-use=${LO_PGO_FILE})
    elseif(NOT LO_PGO_MODE STREQUAL "off")
        message(FATAL_ERROR "LO_PGO_MODE must be 'off', 'generate', or 'use'; got '${LO_PGO_MODE}'")
    endif()

    # BOLT relocations
    if(LO_BOLT_READY)
        if(UNIX)
            add_compile_options(-gline-tables-only)
            add_link_options(-Wl,--emit-relocs)
        else()
            message(WARNING "LO_BOLT_READY is only supported on Linux ELF targets")
        endif()
    endif()

    # Diagnostic frame pointer / symbols
    if(LO_PROFILE_DIAGNOSTIC)
        if(MSVC)
            add_compile_options(/Oy-)
        else()
            add_compile_options(-fno-omit-frame-pointer -gline-tables-only)
        endif()
    endif()

    # Prefer lld on Linux Clang
    if(UNIX AND NOT APPLE)
        find_program(LO_LLD_EXECUTABLE NAMES ld.lld lld)
        if(LO_LLD_EXECUTABLE)
            add_link_options(-fuse-ld=lld)
        endif()
    endif()
endif()
