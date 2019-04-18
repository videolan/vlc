/*****************************************************************************
 * data.c: data URL scheme
 *****************************************************************************
 * Copyright (C) 2021 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_strings.h>
#include <vlc_url.h>

struct access_data {
    size_t type_length;
    size_t length;
    size_t offset;
    char *data;
};

static ssize_t Read(stream_t *access, void *buf, size_t len)
{
    struct access_data *sys = access->p_sys;
    size_t avail = sys->length - sys->offset;

    assert(sys->offset <= sys->length);

    if (len > avail)
        len = avail;

    memcpy(buf, sys->data + sys->offset, len);
    sys->offset += len;
    return len;
}

static int Seek(stream_t *access, uint64_t position)
{
    struct access_data *sys = access->p_sys;

    sys->offset = (position <= sys->length) ? position : sys->length;
    return VLC_SUCCESS;
}

static int Control(stream_t *access, int query, va_list args)
{
    struct access_data *sys = access->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = true;
            return VLC_SUCCESS;

        case STREAM_GET_SIZE:
            *va_arg(args, uint64_t *) = sys->length;
            return VLC_SUCCESS;

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) =  DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;

        case STREAM_GET_CONTENT_TYPE: {
            char *type = strndup(access->psz_url + 5, sys->type_length);

            *va_arg(args, char **) = type;
            return likely(type != NULL) ? VLC_SUCCESS : VLC_ENOMEM;
        }
    
        case STREAM_SET_PAUSE_STATE:
            return VLC_SUCCESS;
    }
    return VLC_ENOTSUP;
}

static int Open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    const char *url = access->psz_url;

    if (strncmp(url, "data:", 5))
        return VLC_ENOTSUP;

    url += 5;

    struct access_data *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    const char *data = strchr(url, ',');
    if (unlikely(data == NULL)) {
        msg_Err(obj, "invalid data URL");
        return VLC_EINVAL;
    }

    sys->data = vlc_uri_decode_duplicate(data + 1);
    if (unlikely(sys->data == NULL))
        return -errno;

    sys->type_length = data - url;
    sys->length = strlen(sys->data);
    sys->offset = 0;

    if ((data - url) >= 7 && strncmp(data - 7, ";base64", 7) == 0) {
        sys->type_length -= 7; /* ";base64" is not part of the type */

        size_t size = sys->length - (sys->length / 4);
        char *d = malloc(size);

        if (likely(d != NULL))
            sys->length = vlc_b64_decode_binary_to_buffer(d, size, sys->data);
        free(sys->data);
        sys->data = d;

        if (unlikely(d == NULL))
            return VLC_ENOMEM;
    }

    access->pf_read = Read;
    access->pf_block = NULL;
    access->pf_seek = Seek;
    access->pf_control = Control;
    access->p_sys = sys;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    struct access_data *sys = access->p_sys;

    free(sys->data);
}

vlc_module_begin()
    set_shortname(N_("data"))
    set_description(N_("data URI scheme"))
    set_subcategory(SUBCAT_INPUT_ACCESS)

    set_capability("access", 0)
    set_callbacks(Open, Close)
    add_shortcut("data")
vlc_module_end()
