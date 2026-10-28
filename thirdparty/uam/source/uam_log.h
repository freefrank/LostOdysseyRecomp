#pragma once
// SwitchWakerHD patches 3 and 4 (see PATCHES.md): uam's diagnostics go through one settable callback
// instead of stderr, and fatal errors reachable from DekoCompiler::CompileGlsl fail the compile
// (longjmp back to it) instead of calling exit() or abort().
#include <stdarg.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

// receives whole messages (usually newline terminated); NULL restores the default (stderr)
typedef void (*uam_log_fn)(const char* msg, void* user);
void uam_set_log_callback(uam_log_fn fn, void* user);

void uam_logf(const char* fmt, ...)
#if defined(__GNUC__)
	__attribute__((format(printf, 1, 2)))
#endif
	;
void uam_vlogf(const char* fmt, va_list ap);

// set by CompileGlsl while it runs (NULL otherwise); uam_fatal() logs and jumps there, or aborts
// when nothing is armed (uam used outside CompileGlsl)
extern jmp_buf* uam_fatal_jmp;
#if defined(__GNUC__)
__attribute__((noreturn, format(printf, 1, 2)))
#endif
void uam_fatal(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
