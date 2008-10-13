/*****************************************************************************
 * stream.h: Input stream functions
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _INPUT_STREAM_H
#define _INPUT_STREAM_H 1

#include <vlc_common.h>
#include <vlc_stream.h>

/**
 * stream_t definition
 */
struct stream_t
{
    VLC_COMMON_MEMBERS

    /*block_t *(*pf_block)  ( stream_t *, int i_size );*/
    int      (*pf_read)   ( stream_t *, void *p_read, unsigned int i_read );
    int      (*pf_peek)   ( stream_t *, const uint8_t **pp_peek, unsigned int i_peek );
    int      (*pf_control)( stream_t *, int i_query, va_list );
    void     (*pf_destroy)( stream_t *);

    stream_sys_t *p_sys;

    /* UTF-16 and UTF-32 file reading */
    vlc_iconv_t     conv;
    int             i_char_width;
    bool            b_little_endian;
};

#include <libvlc.h>

static inline stream_t *vlc_stream_create( vlc_object_t *obj )
{
    return (stream_t *)vlc_custom_create( obj, sizeof(stream_t),
                                          VLC_OBJECT_GENERIC, "stream" );
}

/* */
stream_t *stream_AccessNew( access_t *p_access, bool );
void stream_AccessDelete( stream_t *s );
void stream_AccessReset( stream_t *s );
void stream_AccessUpdate( stream_t *s );

#endif

