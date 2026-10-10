# switch-aarch64

Nintendo Switch (devkitA64 GCC, newlib + libnx) configuration for the pinned
Xenia FFmpeg fork. Derived from `android-aarch64/config.h` (same ISA, same
NEON/armv8 assembly) with the operating-system features newlib does not
provide turned off: HAVE_LINUX_PERF_EVENT_H, HAVE_SYS_RESOURCE_H, HAVE_ARC4RANDOM, HAVE_GETRUSAGE, HAVE_MMAP, HAVE_MPROTECT, HAVE_SCHED_GETAFFINITY, HAVE_SETRLIMIT, HAVE_SYSCONF, HAVE_ISATTY, HAVE_GETAUXVAL, HAVE_ELF_AUX_INFO, HAVE_STRERROR_R, HAVE_POLL_H, HAVE_STRUCT_POLLFD, HAVE_DLOPEN, HAVE_PRCTL.

Only the XMA decoder path of libavcodec/libavutil is built (thirdparty/ffmpeg.cmake),
so none of these switch a codec on or off.
