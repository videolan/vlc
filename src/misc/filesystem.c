// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * filesystem.c: filesystem helpers
 *****************************************************************************
 * Copyright © 2024 VLC authors, VideoLAN and Videolabs
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

/**
 * Create all directories in the given path if missing.
 */
int vlc_mkdir_parent(const char *dirname, mode_t mode)
{
    int ret = vlc_mkdir(dirname, mode);
    if (ret == 0 || errno == EEXIST) {
        return 0;
    } else if (errno != ENOENT) {
        return -1;
    }

    char *path = strdup(dirname);
    if (path == NULL) {
        return -1;
    }

    char *ptr = path + 1;
    while (*ptr) {
        ptr = strchr(ptr, DIR_SEP_CHAR);
        if (ptr == NULL) {
            break;
        }
        *ptr = '\0';
        if (vlc_mkdir(path, mode) != 0) {
            if (errno != EEXIST) {
                free(path);
                return -1;
            }
        }
        *ptr = DIR_SEP_CHAR;
        ptr++;
    }
    ret = vlc_mkdir(path, mode);
    if (errno == EEXIST) {
        ret = 0;
    }
    free(path);
    return ret;
}
