# Development aid, not a release toolchain: compiles the Switch code paths with
# a stock aarch64-linux-gnu GCC (Ubuntu g++-aarch64-linux-gnu) and the libnx
# headers, so most compile errors can be found on a machine without devkitPro.
# Linux macros are removed so the runtime takes its Switch branches; glibc
# still supplies the C library headers, so a missing newlib function is not
# caught here. Nothing links.
#
#   git clone --depth 1 https://github.com/switchbrew/libnx /path/to/libnx
#   cmake -S . -B out/build/switch-check -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=tools/switch/check-toolchain.cmake \
#     -DLO_SWITCH_CHECK_LIBNX=/path/to/libnx
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(LO_TARGET_SWITCH ON CACHE BOOL "" FORCE)
set(LO_SWITCH_CHECK_ONLY ON CACHE BOOL "" FORCE)
set(NINTENDO_SWITCH TRUE)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc-14)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++-14)
set(_flags "-march=armv8-a+crc+crypto -mtune=cortex-a57 -D__SWITCH__ -U__linux__ -U__linux -Ulinux -U__gnu_linux__ -U__unix__ -U__unix -Uunix -Wno-psabi")
set(CMAKE_C_FLAGS_INIT "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")
if(LO_SWITCH_CHECK_LIBNX)
    include_directories(SYSTEM "${LO_SWITCH_CHECK_LIBNX}/nx/include" "${CMAKE_CURRENT_LIST_DIR}/check-include")
endif()
