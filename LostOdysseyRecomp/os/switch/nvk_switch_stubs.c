// From UnleashedRecomp-NX (ChanseyIsTheBest/UnleashedRecomp-NX, GPL-3.0), os/switch/nvk_switch_stubs.c.
// Compiled only into the Switch build (LostOdysseyRecomp/CMakeLists.txt).
// Stubs for symbols the Mesa-Switch NVK static closure references but that
// neither libnx nor newlib provide on Horizon.
//
// Three groups:
//  1. glibc-only POSIX calls pulled in by the prebuilt Rust std inside
//     libnak_rs.a/libnil.a (aarch64-unknown-linux-gnu). None of these paths
//     execute during normal shader compilation; they exist because Rust std
//     is linked as a monolithic object. They fail with ENOSYS unless Rust std
//     needs a real answer to stay on its fallback paths.
//  2. nouveau_ws_* — the Linux DRM winsys, compiled out of this Mesa build
//     but still referenced by the dead nvkmd_nouveau backend objects.
//  3. vk_drm_syncobj_finish — DRM syncobj path of the Vulkan runtime, unused
//     with the nvkmd_switch backend.
//
// Everything is a strong definition; the closure itself defines none of these
// (verified with an nm sweep over all 23 member archives, 2026-07-03).

#ifdef __SWITCH__

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// newlib implements these as macros; we need linkable functions because the
// prebuilt Rust std references them as symbols.
#undef sigaddset
#undef sigemptyset

// Weak: if the driver archive ever brings its own definition (newer mesa-switch
// builds compile more of the winsys), the linker takes that one instead of
// failing on a duplicate; otherwise these are used exactly as before.
#define STUB_ENOSYS(ret_type, name, args) \
    __attribute__((weak)) ret_type name args { errno = ENOSYS; return (ret_type)-1; }

// ---------------------------------------------------------------------------
// 1. Rust std glibc shims
// ---------------------------------------------------------------------------

// Rust std probes the glibc version to decide whether modern syscalls
// (statx, getrandom, ...) are worth trying. Report something ancient so it
// stays on the conservative fallback paths.
__attribute__((weak)) const char *gnu_get_libc_version(void) { return "2.17"; }

__attribute__((weak)) long sysconf(int name)
{
    switch (name)
    {
        case 8:  /* _SC_PAGESIZE (glibc) */
        case 30: /* _SC_PAGESIZE (newlib) */
            return 0x1000;
        case 83: /* _SC_NPROCESSORS_CONF (glibc) */
        case 84: /* _SC_NPROCESSORS_ONLN (glibc) */
            return 3; // application cores available to homebrew
        default:
            errno = EINVAL;
            return -1;
    }
}

__attribute__((weak)) int clock_nanosleep(int clock_id, int flags, const struct timespec *req, struct timespec *rem)
{
    (void)clock_id;
    struct timespec relative = *req;

    if (flags == 1 /* TIMER_ABSTIME */)
    {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
            return EINVAL;
        relative.tv_sec = req->tv_sec - now.tv_sec;
        relative.tv_nsec = req->tv_nsec - now.tv_nsec;
        if (relative.tv_nsec < 0)
        {
            relative.tv_sec -= 1;
            relative.tv_nsec += 1000000000L;
        }
        if (relative.tv_sec < 0)
            return 0;
    }

    if (nanosleep(&relative, rem) != 0)
        return errno;
    return 0;
}

// Signal management: nothing on Horizon raises POSIX signals, so pretending
// success keeps Rust std's optional signal-handler setup happy without
// side effects.
__attribute__((weak)) int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)signum; (void)act;
    if (oldact != NULL)
        memset(oldact, 0, sizeof(*oldact));
    return 0;
}
__attribute__((weak)) int sigaltstack(const stack_t *ss, stack_t *old_ss) { (void)ss; (void)old_ss; return 0; }
__attribute__((weak)) int sigaddset(sigset_t *set, int signum) { (void)set; (void)signum; return 0; }
__attribute__((weak)) int sigemptyset(sigset_t *set) { if (set != NULL) memset(set, 0, sizeof(*set)); return 0; }

// Identity/process queries with harmless constant answers.
__attribute__((weak)) uid_t getuid(void) { return 1; }
__attribute__((weak)) pid_t getppid(void) { return 1; }
__attribute__((weak)) int pthread_setname_np(void *thread, const char *name) { (void)thread; (void)name; return 0; }
__attribute__((weak)) void *dlsym(void *handle, const char *symbol) { (void)handle; (void)symbol; return NULL; }

// pthread_getattr_np: only queried when Rust std computes stack guards for
// threads it did not create. Failing is handled (std skips guard checks).
__attribute__((weak)) int pthread_getattr_np(void *thread, void *attr) { (void)thread; (void)attr; return ENOSYS; }

// Rust's available_parallelism: failure falls back to 1, which is fine for
// the NAK compiler (it is invoked per-shader from app threads anyway).
STUB_ENOSYS(int, sched_getaffinity, (pid_t pid, size_t cpusetsize, void *mask))

// Memory protection: Rust std only calls this when building guard pages for
// stacks it allocated itself via mmap, which newlib's mmap already refuses.
__attribute__((weak)) int mprotect(void *addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }

// File I/O variants Rust std may attempt before falling back; all fail
// cleanly. The glibc struct layouts (stat64/dirent64) differ from newlib's,
// so faking success here would corrupt memory — ENOSYS is the only safe
// answer.
STUB_ENOSYS(int, fstatat64, (int dirfd_, const char *pathname, void *statbuf, int flags))
STUB_ENOSYS(int, lstat64, (const char *pathname, void *statbuf))
STUB_ENOSYS(int, openat64, (int dirfd_, const char *pathname, int flags, ...))
STUB_ENOSYS(long, ftruncate64, (int fd, int64_t length))
STUB_ENOSYS(long, pread64, (int fd, void *buf, size_t count, int64_t offset))
STUB_ENOSYS(long, pwrite64, (int fd, const void *buf, size_t count, int64_t offset))
STUB_ENOSYS(long, preadv, (int fd, const void *iov, int iovcnt, int64_t offset))
STUB_ENOSYS(long, pwritev, (int fd, const void *iov, int iovcnt, int64_t offset))
STUB_ENOSYS(long, readv, (int fd, const void *iov, int iovcnt))
STUB_ENOSYS(long, sendfile64, (int out_fd, int in_fd, int64_t *offset, size_t count))
STUB_ENOSYS(long, splice, (int fd_in, int64_t *off_in, int fd_out, int64_t *off_out, size_t len, unsigned int flags))
STUB_ENOSYS(long, copy_file_range, (int fd_in, int64_t *off_in, int fd_out, int64_t *off_out, size_t len, unsigned int flags))
__attribute__((weak)) void *readdir64(void *dirp) { (void)dirp; errno = ENOSYS; return NULL; }
__attribute__((weak)) void *fdopendir(int fd) { (void)fd; errno = ENOSYS; return NULL; }
STUB_ENOSYS(int, dirfd, (void *dirp))
STUB_ENOSYS(int, unlinkat, (int dirfd_, const char *pathname, int flags))
STUB_ENOSYS(int, linkat, (int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags))
STUB_ENOSYS(int, futimens, (int fd, const struct timespec times[2]))
STUB_ENOSYS(int, utimensat, (int dirfd_, const char *pathname, const struct timespec times[2], int flags))
STUB_ENOSYS(int, fdatasync, (int fd))
// LostOdysseyRecomp: no /proc/self/exe; callers fall back to the SD card folder.
STUB_ENOSYS(long, readlink, (const char *path, char *buf, size_t size))

// LostOdysseyRecomp: the shared runtime takes advisory flock()s on its shader
// cache, logs and import lock to keep a second PC instance out. The console
// runs one process, so locking always succeeds (an ENOSYS stub would make
// those files fail to open).
__attribute__((weak)) int flock(int fd, int operation) { (void)fd; (void)operation; return 0; }
STUB_ENOSYS(int, mkfifo, (const char *pathname, mode_t mode))
STUB_ENOSYS(int, pipe2, (int pipefd[2], int flags))
STUB_ENOSYS(int, accept4, (int sockfd, void *addr, void *addrlen, int flags))

// Ownership/credentials — no multi-user model on Horizon.
STUB_ENOSYS(int, chown, (const char *pathname, uid_t owner, gid_t group))
STUB_ENOSYS(int, fchown, (int fd, uid_t owner, gid_t group))
STUB_ENOSYS(int, lchown, (const char *pathname, uid_t owner, gid_t group))
STUB_ENOSYS(int, chroot, (const char *path))
STUB_ENOSYS(int, setuid, (uid_t uid))
STUB_ENOSYS(int, setgid, (gid_t gid))
STUB_ENOSYS(int, setgroups, (int size, const gid_t *list))
STUB_ENOSYS(int, getpwuid_r, (uid_t uid, void *pwd, char *buf, size_t buflen, void **result))

// Process control — no fork/exec on Horizon.
STUB_ENOSYS(int, execvp, (const char *file, char *const argv[]))
STUB_ENOSYS(int, posix_spawnp, (pid_t *pid, const char *file, const void *file_actions, const void *attrp, char *const argv[], char *const envp[]))
STUB_ENOSYS(int, posix_spawn_file_actions_init, (void *file_actions))
STUB_ENOSYS(int, posix_spawn_file_actions_destroy, (void *file_actions))
STUB_ENOSYS(int, posix_spawn_file_actions_adddup2, (void *file_actions, int fd, int newfd))
STUB_ENOSYS(int, posix_spawnattr_init, (void *attr))
STUB_ENOSYS(int, posix_spawnattr_destroy, (void *attr))
STUB_ENOSYS(int, posix_spawnattr_setflags, (void *attr, short flags))
STUB_ENOSYS(int, posix_spawnattr_setpgroup, (void *attr, pid_t pgroup))
STUB_ENOSYS(int, posix_spawnattr_setsigdefault, (void *attr, const void *sigdefault))
STUB_ENOSYS(pid_t, waitpid, (pid_t pid, int *wstatus, int options))
STUB_ENOSYS(int, waitid, (int idtype, unsigned id, void *infop, int options))
STUB_ENOSYS(int, killpg, (int pgrp, int sig))
STUB_ENOSYS(int, setpgid, (pid_t pid, pid_t pgid))
STUB_ENOSYS(pid_t, setsid, (void))
STUB_ENOSYS(int, pause, (void))
STUB_ENOSYS(int, __res_init, (void))

// ---------------------------------------------------------------------------
// 2. Dead Linux DRM winsys (nvkmd_nouveau backend objects reference these,
//    but the backend never probes successfully on Horizon).
// ---------------------------------------------------------------------------

__attribute__((weak)) void nouveau_ws_bo_destroy(void *bo) { (void)bo; }
__attribute__((weak)) int nouveau_ws_bo_dma_buf(void *bo, int *fd) { (void)bo; (void)fd; errno = ENOSYS; return -1; }
__attribute__((weak)) void *nouveau_ws_bo_from_dma_buf(void *dev, int fd, void *size_out) { (void)dev; (void)fd; (void)size_out; return NULL; }
__attribute__((weak)) void *nouveau_ws_bo_new_tiled(void *dev, uint64_t size, uint64_t align, uint8_t pte_kind, uint16_t tile_mode, unsigned flags) { (void)dev; (void)size; (void)align; (void)pte_kind; (void)tile_mode; (void)flags; return NULL; }
__attribute__((weak)) int nouveau_ws_context_create(void *dev, unsigned flags, void **out) { (void)dev; (void)flags; (void)out; errno = ENOSYS; return -1; }
__attribute__((weak)) void nouveau_ws_context_destroy(void *ctx) { (void)ctx; }
__attribute__((weak)) void nouveau_ws_device_destroy(void *dev) { (void)dev; }
__attribute__((weak)) uint64_t nouveau_ws_device_timestamp(void *dev) { (void)dev; return 0; }
__attribute__((weak)) uint64_t nouveau_ws_device_vram_used(void *dev) { (void)dev; return 0; }

// ---------------------------------------------------------------------------
// 3. Vulkan runtime DRM syncobj path (unused with nvkmd_switch).
// ---------------------------------------------------------------------------

__attribute__((weak)) void vk_drm_syncobj_finish(void *device, void *sync) { (void)device; (void)sync; }

// ---------------------------------------------------------------------------
// 4. libelf (current mesa-switch builds). nv_cubin.c, the NVIDIA CUBIN parser,
//    is part of the static driver and references libelf, which devkitPro does
//    not ship. The parser is never used on Horizon. Weak, so a real libelf (or a
//    driver that brings its own) takes precedence; unused with older drivers.
//    If one of these were ever reached, elf_version() reporting "no version"
//    and NULL/-1 everywhere else make the caller give up cleanly.
// ---------------------------------------------------------------------------

#define ELF_STUB __attribute__((weak))

ELF_STUB unsigned int elf_version(unsigned int version) { (void)version; return 0; /* EV_NONE */ }
ELF_STUB void *elf_memory(char *image, size_t size) { (void)image; (void)size; return NULL; }
ELF_STUB int elf_end(void *elf) { (void)elf; return 0; }
ELF_STUB int elf_kind(void *elf) { (void)elf; return 0; /* ELF_K_NONE */ }
ELF_STUB int elf_errno(void) { return 0; }
ELF_STUB const char *elf_errmsg(int error) { (void)error; return "libelf is not available on Horizon"; }
ELF_STUB void *elf64_getehdr(void *elf) { (void)elf; return NULL; }
ELF_STUB void *elf64_getshdr(void *section) { (void)section; return NULL; }
ELF_STUB void *elf_getscn(void *elf, size_t index) { (void)elf; (void)index; return NULL; }
ELF_STUB void *elf_nextscn(void *elf, void *section) { (void)elf; (void)section; return NULL; }
ELF_STUB void *elf_getdata(void *section, void *data) { (void)section; (void)data; return NULL; }
ELF_STUB char *elf_strptr(void *elf, size_t section, size_t offset) { (void)elf; (void)section; (void)offset; return NULL; }
ELF_STUB int elf_getshdrstrndx(void *elf, size_t *index)
{
    (void)elf;
    if (index != NULL)
        *index = 0;
    return -1;
}

#undef ELF_STUB

// ---------------------------------------------------------------------------
// 5. EGL (devkitPro's SDL2). SDL's Switch video driver keeps a table of EGL
//    entry points for OpenGL windows, so libSDL2.a references them as soon as
//    SDL video is used. This app renders with Vulkan (its window is created
//    without SDL_WINDOW_OPENGL), so SDL never calls them. Linking devkitPro's
//    libEGL.a instead would pull in its older Mesa next to the NVK driver's,
//    with clashing symbol names. Weak failure stubs keep both out: should SDL
//    ever try, it gets "no display" / EGL_FALSE and reports an SDL error.
// ---------------------------------------------------------------------------

#define EGL_STUB __attribute__((weak))
#define EGL_STUB_FALSE 0u              // EGL_FALSE
#define EGL_STUB_NOT_INITIALIZED 0x3001 // EGL_NOT_INITIALIZED
#define EGL_STUB_NONE 0x3038u          // EGL_NONE

EGL_STUB void *eglGetDisplay(void *display) { (void)display; return NULL; }
EGL_STUB void *eglGetPlatformDisplay(unsigned int platform, void *display, const intptr_t *attributes)
{ (void)platform; (void)display; (void)attributes; return NULL; }
EGL_STUB unsigned int eglInitialize(void *display, int *major, int *minor)
{ (void)display; (void)major; (void)minor; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglTerminate(void *display) { (void)display; return EGL_STUB_FALSE; }
EGL_STUB void *eglGetProcAddress(const char *name) { (void)name; return NULL; }
EGL_STUB unsigned int eglChooseConfig(void *display, const int *attributes, void **configs, int size, int *count)
{ (void)display; (void)attributes; (void)configs; (void)size; if (count != NULL) *count = 0; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglGetConfigs(void *display, void **configs, int size, int *count)
{ (void)display; (void)configs; (void)size; if (count != NULL) *count = 0; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglGetConfigAttrib(void *display, void *config, int attribute, int *value)
{ (void)display; (void)config; (void)attribute; (void)value; return EGL_STUB_FALSE; }
EGL_STUB void *eglCreateContext(void *display, void *config, void *share, const int *attributes)
{ (void)display; (void)config; (void)share; (void)attributes; return NULL; }
EGL_STUB unsigned int eglDestroyContext(void *display, void *context) { (void)display; (void)context; return EGL_STUB_FALSE; }
EGL_STUB void *eglCreatePbufferSurface(void *display, void *config, const int *attributes)
{ (void)display; (void)config; (void)attributes; return NULL; }
EGL_STUB void *eglCreateWindowSurface(void *display, void *config, void *window, const int *attributes)
{ (void)display; (void)config; (void)window; (void)attributes; return NULL; }
EGL_STUB unsigned int eglDestroySurface(void *display, void *surface) { (void)display; (void)surface; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglQuerySurface(void *display, void *surface, int attribute, int *value)
{ (void)display; (void)surface; (void)attribute; (void)value; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglMakeCurrent(void *display, void *draw, void *read, void *context)
{ (void)display; (void)draw; (void)read; (void)context; return EGL_STUB_FALSE; }
EGL_STUB void *eglGetCurrentContext(void) { return NULL; }
EGL_STUB void *eglGetCurrentDisplay(void) { return NULL; }
EGL_STUB void *eglGetCurrentSurface(int which) { (void)which; return NULL; }
EGL_STUB unsigned int eglSwapBuffers(void *display, void *surface) { (void)display; (void)surface; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglSwapInterval(void *display, int interval) { (void)display; (void)interval; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglWaitNative(int engine) { (void)engine; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglWaitGL(void) { return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglBindAPI(unsigned int api) { (void)api; return EGL_STUB_FALSE; }
EGL_STUB unsigned int eglQueryAPI(void) { return EGL_STUB_NONE; }
EGL_STUB unsigned int eglReleaseThread(void) { return EGL_STUB_FALSE; }
EGL_STUB const char *eglQueryString(void *display, int name) { (void)display; (void)name; return NULL; }
EGL_STUB int eglGetError(void) { return EGL_STUB_NOT_INITIALIZED; }

#undef EGL_STUB
#undef EGL_STUB_FALSE
#undef EGL_STUB_NOT_INITIALIZED
#undef EGL_STUB_NONE

#endif // __SWITCH__
