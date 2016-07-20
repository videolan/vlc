/*****************************************************************************
 * stream_memory.c: stream_t wrapper around memory buffer
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 * $Id$
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

#include "stream.h"

struct stream_sys_t
{
    size_t    i_pos;      /* Current reading offset */
    size_t    i_size;
    uint8_t  *p_buffer;
};

static ssize_t Read( stream_t *, void *p_read, size_t i_read );
static int Seek( stream_t *, uint64_t );
static int  Control( stream_t *, int i_query, va_list );

static void stream_MemoryPreserveDelete(stream_t *s)
{
    free(s->p_sys);
}

static void stream_MemoryDelete(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    free(sys->p_buffer);
    stream_MemoryPreserveDelete(s);
}

stream_t *(vlc_stream_MemoryNew)(vlc_object_t *p_this, uint8_t *p_buffer,
                                 size_t i_size, bool preserve)
{
    stream_t *s = vlc_stream_CommonNew( p_this,
                                        preserve ? stream_MemoryPreserveDelete
                                                 : stream_MemoryDelete );
    stream_sys_t *p_sys;

    if( !s )
        return NULL;

    s->p_sys = p_sys = malloc( sizeof( stream_sys_t ) );
    if( !s->p_sys )
    {
        stream_CommonDelete( s );
        return NULL;
    }
    p_sys->i_pos = 0;
    p_sys->i_size = i_size;
    p_sys->p_buffer = p_buffer;

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
    stream_sys_t *p_sys = s->p_sys;

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
            *va_arg( args, int64_t * ) = 0;
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
    stream_sys_t *p_sys = s->p_sys;

    if( i_read > p_sys->i_size - p_sys->i_pos )
        i_read = p_sys->i_size - p_sys->i_pos;
    if ( p_read )
        memcpy( p_read, p_sys->p_buffer + p_sys->i_pos, i_read );
    p_sys->i_pos += i_read;
    return i_read;
}

static int Seek( stream_t *s, uint64_t offset )
{
    stream_sys_t *p_sys = s->p_sys;

    if( offset > p_sys->i_size )
        offset = p_sys->i_size;

    p_sys->i_pos = offset;
    return VLC_SUCCESS;
}
