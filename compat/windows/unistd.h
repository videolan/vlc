// Copyright © 2023 VideoLabs, VLC authors and VideoLAN
// SPDX-License-Identifier: ISC
//
// Authors: Steve Lhomme <robux4@videolabs.io>

#ifndef WINSDK_UNISTD_H__
#define WINSDK_UNISTD_H__

// Windows is not a real POSIX system and doesn't provide this header
// provide a dummy one so the code can compile

#if defined(_GAMING_XBOX_SCARLETT) || defined(_GAMING_XBOX_XBOXONE) || defined(_XBOX_ONE)
# define _CRT_DECLARE_NONSTDC_NAMES 0
#else
// many functions commonly found in unistd.h are found in io.h and process.h
# define _CRT_DECLARE_NONSTDC_NAMES 1
#endif
#include <io.h>
#include <process.h>

// defines corresponding to stdin/stdout/stderr without the __acrt_iob_func() call
#define	STDIN_FILENO  0
#define	STDOUT_FILENO 1
#define	STDERR_FILENO 2

// _access() doesn't function the same as access(), but this should work
#define R_OK  04

// _getpid() exists but it returns an int, not a pid_t
typedef int pid_t;


#endif // WINSDK_UNISTD_H__
