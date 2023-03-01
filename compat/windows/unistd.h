// Copyright © 2023 VideoLabs, VLC authors and VideoLAN
// SPDX-License-Identifier: ISC
//
// Authors: Steve Lhomme <robux4@videolabs.io>

#ifndef WINSDK_UNISTD_H__
#define WINSDK_UNISTD_H__

// Windows is not a real POSIX system and doesn't provide this header
// provide a dummy one so the code can compile

// many functions commonly found in unistd.h are found in io.h and process.h
#include <io.h>
#include <process.h>

// defines corresponding to stdin/stdout/stderr without the __acrt_iob_func() call
#define    STDIN_FILENO  0
#define    STDOUT_FILENO 1
#define    STDERR_FILENO 2

// _access() doesn't function the same as access(), but this should work
#define R_OK  04

// _getpid() exists but it returns an int, not a pid_t
typedef int pid_t;

// redirect missing functions from the GDK
#if defined(_CRT_INTERNAL_NONSTDC_NAMES) && !_CRT_INTERNAL_NONSTDC_NAMES
static inline FILE *fdopen(int fd, const char *mode)
{
    return _fdopen(fd, mode);
}
static inline int close(int fd)
{
    return _close(fd);
}
static inline int read(int fd, void *dst, unsigned int dst_size)
{
    return _read(fd, dst, dst_size);
}
static inline int write(int fd, const void *src, unsigned int src_size)
{
    return _write(fd, src, src_size);
}
static inline int setmode(int fd, int mode)
{
    return _setmode(fd, mode);
}
static inline int dup(int fd)
{
    return _dup(fd);
}
static inline int dup2(int src, int dst)
{
    return _dup2(src, dst);
}
#endif // !_CRT_INTERNAL_NONSTDC_NAMES



#endif // WINSDK_UNISTD_H__
