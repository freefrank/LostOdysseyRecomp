/* Nintendo Switch only (LostOdysseyRecomp/CMakeLists.txt adds this folder to
 * the runtime's include path). Horizon has no dynamic loader and newlib ships
 * no <dlfcn.h>, but third-party headers include it unconditionally on
 * non-Windows hosts (DXC's dxcapi.h). Every call fails cleanly. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#define RTLD_LAZY   0x0001
#define RTLD_NOW    0x0002
#define RTLD_GLOBAL 0x0100
#define RTLD_LOCAL  0x0000
static inline void* dlopen(const char* file, int mode) { (void)file; (void)mode; return 0; }
static inline void* dlsym(void* handle, const char* name) { (void)handle; (void)name; return 0; }
static inline int dlclose(void* handle) { (void)handle; return 0; }
static inline char* dlerror(void) { return (char*)"dynamic loading is not available on Nintendo Switch"; }
#ifdef __cplusplus
}
#endif
