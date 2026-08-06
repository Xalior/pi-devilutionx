//
// circle_stubs.cpp — the host-specific entry points DevilutionX references
// that have nothing to do with SDL.
//
// These are seams, not permanent furniture. When the shim implements one of
// these for real, the way to adopt it is to DELETE the version here: the
// archive is linked whole, so a leftover would become a duplicate-symbol
// error at link time rather than a silent winner over the real thing.
//
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {

// ---------------------------------------------------------------------------
// One POSIX call newlib does not carry
// ---------------------------------------------------------------------------
//
// DevilutionX asks whether a file exists, and whether it can be written to,
// with access(). Circle's newlib has the declaration but no implementation.
// stat() answers the first question exactly; for the second, this filesystem
// is FAT with no permission bits at all, so anything that exists and is not a
// directory is writable — which is the truth about the medium, not an
// assumption about the file.
int access(const char *path, int mode)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;

    if ((mode & W_OK) != 0 && S_ISDIR(st.st_mode))
    {
        errno = EACCES;
        return -1;
    }
    if ((mode & X_OK) != 0 && !S_ISDIR(st.st_mode))
    {
        errno = EACCES;
        return -1;
    }
    return 0;
}

} // extern "C"
