// Copyright © 2023 VideoLabs, VLC authors and VideoLAN
// SPDX-License-Identifier: ISC
//
// Authors: Steve Lhomme <robux4@videolabs.io>

#ifndef WINSDK_DIRENT_H__
#define WINSDK_DIRENT_H__

// Windows is not a real POSIX system and doesn't provide this header
// provide a dummy one so the code can compile

// opaque type for all dirent entries
typedef void DIR;

#define opendir(x) (NULL)

#endif // WINSDK_DIRENT_H__
