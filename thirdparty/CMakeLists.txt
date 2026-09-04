cmake_policy(SET CMP0077 NEW)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

# o1heap's own CMakeLists only builds its tests; wrap the single source file.
add_library(o1heap STATIC "${CMAKE_CURRENT_SOURCE_DIR}/o1heap/o1heap/o1heap.c")
target_include_directories(o1heap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/o1heap/o1heap")
# o1heap defaults to __builtin_clzl, which is 32-bit on Windows (LLP64) and corrupts every bin index.
target_compile_definitions(o1heap PRIVATE O1HEAP_CLZ=__builtin_clzll)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SDL")

# plume is added once the GPU backend lands (phase 2, rendering).
if(LO_BUILD_GPU)
    if(WIN32)
        set(PLUME_D3D12_AGILITY_SDK_ENABLED OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/plume")
endif()
