// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * serializer.h: JSON serializer header
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <vlc_common.h>
#include <vlc_preparser_ipc.h>
#include <vlc_memstream.h>
#include <vlc_vector.h>

#include "../../../demux/json/json.h"

struct serdes_sys {
    struct vlc_preparser_msg_serdes *parent;
    bool bin_data;

    void *userdata;

    int current_type;
    int error;

    struct VLC_VECTOR(uint8_t) attach_data;

    size_t cap;
    size_t size;
    uint8_t buffer[];
};

#define serdes_set_error(err) \
    sys->error = sys->error == VLC_SUCCESS ? err : sys->error

int
serdes_buf_write(struct serdes_sys *sys, const void *_data, size_t size);

int
serdes_buf_puts(struct serdes_sys *sys, const char *data);

int
serdes_buf_putc(struct serdes_sys *sys, char c);

int
serdes_buf_printf(struct serdes_sys *sys, const char *fmt, ...);

int
serdes_buf_flush(struct serdes_sys *sys);

ssize_t
serdes_buf_read(struct serdes_sys *sys, void *_data, size_t size, bool eod);


void
toJSON_vlc_preparser_msg(struct serdes_sys *sys,
                         const struct vlc_preparser_msg *msg);

bool
fromJSON_vlc_preparser_msg(struct serdes_sys *sys,
                           struct vlc_preparser_msg *msg,
                           const struct json_object *obj);


#endif /* SERIALIZER_H */
