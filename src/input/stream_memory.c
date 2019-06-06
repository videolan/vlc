/*****************************************************************************
 * stream_memory.c: stream_t wrapper around memory buffer
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#include <vlc_input.h>
#include "stream.h"

struct vlc_stream_memory_private
{
    size_t    i_pos;      /* Current reading offset */
    size_t    i_size;
    uint8_t  *p_buffer;
};

struct vlc_stream_attachment_private
{
    struct vlc_stream_memory_private memory;
    input_attachment_t *attachment;
};

static ssize_t Read( stream_t *, void *p_read, size_t i_read );
static int Seek( stream_t *, uint64_t );
static int  Control( stream_t *, int i_query, va_list );

static void stream_MemoryPreserveDelete(stream_t *s)
{
    (void) s; /* nothing to do */
}

static void stream_MemoryDelete(stream_t *s)
{
    struct vlc_stream_memory_private *sys = vlc_stream_Private(s);

    free(sys->p_buffer);
}

static void stream_AttachmentDelete(stream_t *s)
{
    struct vlc_stream_attachment_private *sys = vlc_stream_Private(s);

    vlc_input_attachment_Delete(sys->attachment);
    free(s->psz_name);
}

stream_t *(vlc_stream_MemoryNew)(vlc_object_t *p_this, uint8_t *p_buffer,
                                 size_t i_size, bool preserve)
{
    struct vlc_stream_memory_private *p_sys;
    stream_t *s = vlc_stream_CustomNew(p_this,
                                       preserve ? stream_MemoryPreserveDelete
                                                : stream_MemoryDelete,
                                       sizeof (*p_sys), "stream");
    if (unlikely(s == NULL))
        return NULL;

    p_sys = vlc_stream_Private(s);
    p_sys->i_pos = 0;
    p_sys->i_size = i_size;
    p_sys->p_buffer = p_buffer;

    s->pf_read    = Read;
    s->pf_seek    = Seek;
    s->pf_control = Control;

    return s;
}

stream_t *vlc_stream_AttachmentNew(vlc_object_t *p_this,
                                   input_attachment_t *attachment)
{
    struct vlc_stream_attachment_private *p_sys;
    stream_t *s = vlc_stream_CustomNew(p_this, stream_AttachmentDelete,
                                       sizeof (*p_sys), "stream");
    if (unlikely(s == NULL))
        return NULL;

    s->psz_name = strdup("attachment");
    if (unlikely(s->psz_name == NULL))
    {
        stream_CommonDelete(s);
        return NULL;
    }

    p_sys = vlc_stream_Private(s);
    p_sys->memory.i_pos = 0;
    p_sys->memory.i_size = attachment->i_data;
    p_sys->memory.p_buffer = attachment->p_data;
    p_sys->attachment = attachment;

    s->pf_read    = Read;
    s->pf_seek    = Seek;
    s->pf_control = Control;

    return s;
}

/****************************************************************************
 * AStreamControl:
 ****************************************************************************/
static int Control( stream_t *s, int i_query, va_list args )
{
    struct vlc_stream_memory_private *p_sys = vlc_stream_Private(s);

    uint64_t   *pi_64;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            pi_64 = va_arg( args, uint64_t * );
            *pi_64 = p_sys->i_size;
            break;

        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = true;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) = 0;
            break;

        case STREAM_GET_TITLE_INFO:
        case STREAM_GET_TITLE:
        case STREAM_GET_SEEKPOINT:
        case STREAM_GET_META:
        case STREAM_GET_CONTENT_TYPE:
        case STREAM_GET_SIGNAL:
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
            return VLC_EGENERIC;

        case STREAM_SET_PAUSE_STATE:
            break; /* nothing to do */

        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            msg_Err( s, "Hey, what are you thinking? "
                     "DO NOT USE PRIVATE STREAM CONTROLS!!!" );
            return VLC_EGENERIC;

        default:
            msg_Err( s, "invalid vlc_stream_vaControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static ssize_t Read( stream_t *s, void *p_read, size_t i_read )
{
    struct vlc_stream_memory_private *p_sys = vlc_stream_Private(s);

    if( i_read > p_sys->i_size - p_sys->i_pos )
        i_read = p_sys->i_size - p_sys->i_pos;
    if ( p_read )
        memcpy( p_read, p_sys->p_buffer + p_sys->i_pos, i_read );
    p_sys->i_pos += i_read;
    return i_read;
}

static int Seek( stream_t *s, uint64_t offset )
{
    struct vlc_stream_memory_private *p_sys = vlc_stream_Private(s);

    if( offset > p_sys->i_size )
        offset = p_sys->i_size;

    p_sys->i_pos = offset;
    return VLC_SUCCESS;
}
