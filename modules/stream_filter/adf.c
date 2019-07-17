/*****************************************************************************
 * adf.c ADF stream filter
 *****************************************************************************
 * Copyright (C) 2016 VideoLAN Authors
 *
 * Author: Tristan Matthews <tmatth@videolan.org>
 * Based on record.c by: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>

static int Open( vlc_object_t * );

vlc_module_begin()
    set_shortname( "adf" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_capability( "stream_filter", 330 )
    set_description( N_( "ADF stream filter" ) )
    set_callback( Open )
vlc_module_end()

static int Control( stream_t *p_stream, int i_query, va_list args )
{
    return vlc_stream_vaControl( p_stream->s, i_query, args );
}

static ssize_t Read( stream_t *s, void *buffer, size_t i_read )
{
    const ssize_t i_ret = vlc_stream_Read( s->s, buffer, i_read );
    if( i_ret < 1 ) return i_ret;

    uint8_t *p_buffer = buffer;
    static const uint8_t ADF_XOR_MASK = 34;
    for( ssize_t i = 0; i < i_ret; ++i )
        p_buffer[i] ^= ADF_XOR_MASK;
    return i_ret;
}

static int Seek( stream_t *s, uint64_t offset )
{
    return vlc_stream_Seek( s->s, offset );
}

static int Open( vlc_object_t *p_object )
{
    stream_t *p_stream = (stream_t *)p_object;

    /* Require .adf extension unless forced. */
    if( !p_stream->obj.force )
    {
        if( !p_stream->psz_url )
            return VLC_EGENERIC;
        const char *psz_ext = strrchr( p_stream->psz_url, '.' );
        if( !psz_ext || strncmp( psz_ext, ".adf", 4 ) )
            return VLC_EGENERIC;
    }

    const uint8_t *peek;
    if( vlc_stream_Peek( p_stream->s, &peek, 3 ) < 3 )
        return VLC_EGENERIC;

    /* Probe for XOR'd ID3 tag. */
    if( memcmp( peek, "\x6B\x66\x11", 3 ) )
        return VLC_EGENERIC;

    p_stream->pf_read = Read;
    p_stream->pf_seek = Seek;
    p_stream->pf_control = Control;

    return VLC_SUCCESS;
}
