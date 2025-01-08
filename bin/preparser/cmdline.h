// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * preparser.h: internal header of the preparser binary
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef PREPARSER_CMDLINE_H
#define PREPARSER_CMDLINE_H

#include <vlc_common.h>
#include <vlc_preparser.h>

struct preparser_args {
    vlc_tick_t timeout;
    int types;
    bool daemon;
    struct {
        int type;
        float pos;
        vlc_tick_t time;
        int speed;
    } seek;

    struct {
        enum vlc_thumbnailer_format format;
        uint64_t width;
        uint64_t height;
        const char *file_path;
        bool crop;
    } output;

    const char *verbosity;

    int arg_idx;
    bool error;
};

int
preparser_cmdline_Parse(int argc, char *const *argv, struct preparser_args *args);

#endif /* PREPARSER_CMDLINE_H */
