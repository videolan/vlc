/*****************************************************************************
 * json_helper.h:
 *****************************************************************************
 * Copyright (C) 2012-2019 VLC authors, VideoLabs and VideoLAN
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
#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_stream.h>

#include <limits.h>

#include "../../demux/json/json.h"

static inline
char * json_dupstring(const struct json_object *obj, const char *key)
{
    const char *str = json_get_str(obj, key);
    return (str) ? strdup(str) : NULL;
}

static inline
void * json_retrieve_document(vlc_object_t *p_obj, const char *psz_url, size_t *buf_size)
{
    bool saved_no_interact = p_obj->no_interact;
    p_obj->no_interact = true;
    stream_t *p_stream = vlc_stream_NewURL(p_obj, psz_url);

    p_obj->no_interact = saved_no_interact;
    if (p_stream == NULL)
        return NULL;

    stream_t *p_chain = vlc_stream_FilterNew(p_stream, "inflate");
    if(p_chain)
        p_stream = p_chain;

    /* read answer */
    char *p_buffer = NULL;
    *buf_size = 0;
    for(;;)
    {
        int i_read = 65536;

        if(*buf_size >= (SIZE_MAX - i_read - 1))
            break;

        p_buffer = realloc_or_free(p_buffer, 1 + *buf_size + i_read);
        if(unlikely(p_buffer == NULL))
        {
            vlc_stream_Delete(p_stream);
            return NULL;
        }

        i_read = vlc_stream_Read(p_stream, &p_buffer[*buf_size], i_read);
        if(i_read <= 0)
            break;

        *buf_size += i_read;
    }
    vlc_stream_Delete(p_stream);
    p_buffer[*buf_size++] = '\0';

    return p_buffer;
}

struct json_helper_sys {
    struct vlc_logger *logger;
    const char *buffer;
    size_t size;
};

void json_parse_error(void *data, const char *msg)
{
    struct json_helper_sys *sys = data;

    vlc_error(sys->logger, "%s", msg);
}

size_t json_read(void *data, void *buf, size_t size)
{
    struct json_helper_sys *sys = data;

    /* Read the smallest number of byte between size and the string length */
    size_t s = size < sys->size ? size : sys->size; 
    memcpy(buf, sys->buffer, s);

    sys->buffer += s;
    sys->size -= s;
    return s;
}

#endif
