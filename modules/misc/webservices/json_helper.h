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
#include <vlc_stream.h>

#include <limits.h>

#include "json.h"

static inline
const json_value * json_getbyname(const json_value *object, const char *psz_name)
{
    if (object->type != json_object) return NULL;
    for (unsigned int i=0; i < object->u.object.length; i++)
        if (strcmp(object->u.object.values[i].name, psz_name) == 0)
            return object->u.object.values[i].value;
    return NULL;
}

static inline
char * jsongetstring(const json_value *node, const char *key)
{
    node = json_getbyname(node, key);
    if (node && node->type == json_string)
        return node->u.string.ptr;
    return NULL;
}

static inline
char * json_dupstring(const json_value *node, const char *key)
{
    const char *str = jsongetstring(node, key);
    return (str) ? strdup(str) : NULL;
}

static inline
json_value * json_parse_document(vlc_object_t *p_obj, const char *psz_buffer, size_t i_buffer)
{
    json_settings settings;
    char psz_error[128];
    memset (&settings, 0, sizeof (json_settings));
    json_value *root = json_parse_ex(&settings, psz_buffer, i_buffer, psz_error);
    if (root == NULL)
    {
        msg_Warn(p_obj, "Can't parse json data: %s", psz_error);
        goto error;
    }
    if (root->type != json_object)
    {
        msg_Warn(p_obj, "wrong json root node");
        goto error;
    }

    return root;

error:
    if (root) json_value_free(root);
    return NULL;
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

#endif
