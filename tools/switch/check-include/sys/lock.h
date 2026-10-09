/* Check builds only (tools/switch/check-toolchain.cmake): the lock types that
 * devkitPro's newlib <sys/lock.h> provides and libnx's <switch/kernel/mutex.h>
 * uses. The real Switch build uses devkitA64's own header. */
#pragma once
#include <stdint.h>
typedef int32_t _LOCK_T;
typedef struct { _LOCK_T lock; uint32_t thread_tag; uint32_t counter; } _LOCK_RECURSIVE_T;
