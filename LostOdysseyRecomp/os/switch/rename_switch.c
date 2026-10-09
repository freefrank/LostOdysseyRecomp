// rename() that replaces an existing file, as POSIX requires.
//
// Horizon's file system (libnx fsdev -> fsFsRenameFile) refuses to rename onto
// an existing path. The runtime saves almost everything by writing a temporary
// file and renaming it over the old one: game saves (kernel/io/file_system.cpp),
// settings, shader stores and the driver pipeline cache. Linked with
// -Wl,--wrap=rename, so std::filesystem::rename (libstdc++) and std::rename
// both land here.
//
// Not atomic: if the console loses power between the unlink and the second
// rename, the old file is gone but the complete new one is still on the SD
// card under its temporary name.
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int __real_rename(const char* from, const char* to);

int __wrap_rename(const char* from, const char* to)
{
    if (__real_rename(from, to) == 0)
        return 0;
    const int error = errno;
    struct stat source, target;
    if (stat(from, &source) != 0 || stat(to, &target) != 0)
    {
        errno = error;
        return -1;
    }
    // Only a file replaces a file; directories keep the original error.
    if (!S_ISREG(source.st_mode) || !S_ISREG(target.st_mode) || unlink(to) != 0)
    {
        errno = error;
        return -1;
    }
    return __real_rename(from, to);
}
