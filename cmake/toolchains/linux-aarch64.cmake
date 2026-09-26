# Generic Linux AArch64 cross toolchain for LostOdysseyRecomp.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

# The runtime does not use C++20 modules; avoid requiring clang-scan-deps in
# minimal cross-build environments.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Use the Ubuntu/Debian cross GCC installation for crt objects, libstdc++ and
# libgcc while keeping Clang as the project compiler.
set(CMAKE_C_FLAGS_INIT "--gcc-toolchain=/usr")
set(CMAKE_CXX_FLAGS_INIT "--gcc-toolchain=/usr")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--gcc-toolchain=/usr")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--gcc-toolchain=/usr")

set(CMAKE_FIND_ROOT_PATH
    /usr/aarch64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /usr/include/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config must resolve target libraries, not x86-64 host .pc files.
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR}
    "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "/")
