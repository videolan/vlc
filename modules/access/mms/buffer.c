/*****************************************************************************
 * buffer.c: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_access.h>
#include <vlc_charset.h>

#include "asf.h"
#include "buffer.h"

/*****************************************************************************
 * Buffer management functions
 *****************************************************************************/
int var_buffer_initwrite( var_buffer_t *p_buf, int i_default_size )
{
    p_buf->i_size =  ( i_default_size > 0 ) ? i_default_size : 2048;
    p_buf->i_data = 0;
    p_buf->p_data = malloc( p_buf->i_size );
    return p_buf->p_data ? 0 : -1;
}

int var_buffer_reinitwrite( var_buffer_t *p_buf, int i_default_size )
{
    p_buf->i_data = 0;
    if( p_buf->i_size < i_default_size )
    {
        p_buf->i_size = i_default_size;
        free( p_buf->p_data );
        p_buf->p_data = malloc( p_buf->i_size );
    }
    if( !p_buf->p_data )
    {
        p_buf->i_size =  ( i_default_size > 0 ) ? i_default_size : 2048;
        p_buf->p_data = malloc( p_buf->i_size );
    }
    return p_buf->p_data ? 0 : -1;
}

void var_buffer_add8 ( var_buffer_t *p_buf, uint8_t  i_byte )
{
    /* check if there is enough data */
    if( p_buf->i_data >= p_buf->i_size )
    {
        p_buf->i_size += 1024;
        p_buf->p_data = xrealloc( p_buf->p_data, p_buf->i_size );
    }
    p_buf->p_data[p_buf->i_data] = i_byte&0xff;
    p_buf->i_data++;
}

void var_buffer_add16( var_buffer_t *p_buf, uint16_t i_word )
{
    var_buffer_add8( p_buf, i_word&0xff );
    var_buffer_add8( p_buf, ( i_word >> 8 )&0xff );
}

void var_buffer_add32( var_buffer_t *p_buf, uint32_t i_dword )
{
    var_buffer_add16( p_buf, i_dword&0xffff );
    var_buffer_add16( p_buf, ( i_dword >> 16 )&0xffff );
}

void var_buffer_add64( var_buffer_t *p_buf, uint64_t i_long )
{
    var_buffer_add32( p_buf, i_long&0xffffffff );
    var_buffer_add32( p_buf, ( i_long >> 32 )&0xffffffff );
}

void var_buffer_addmemory( var_buffer_t *p_buf, void *p_mem, int i_mem )
{
    /* check if there is enough data */
    if( p_buf->i_data + i_mem >= p_buf->i_size )
    {
        p_buf->i_size += i_mem + 1024;
        p_buf->p_data = xrealloc( p_buf->p_data, p_buf->i_size );
    }

    memcpy( p_buf->p_data + p_buf->i_data, p_mem, i_mem );
    p_buf->i_data += i_mem;
}

void var_buffer_addUTF16( access_t  *p_access, var_buffer_t *p_buf, const char *p_str )
{
    uint16_t *p_out;
    size_t i_out;

    if( p_str != NULL )
#ifdef WORDS_BIGENDIAN
        p_out = ToCharset( "UTF-16BE", p_str, &i_out );
#else
        p_out = ToCharset( "UTF-16LE", p_str, &i_out );
#endif
    else
        p_out = NULL;
    if( p_out == NULL )
    {
        msg_Err( p_access, "UTF-16 conversion failed" );
        i_out = 0;
    }

    i_out /= 2;
    for( size_t i = 0; i < i_out; i ++ )
        var_buffer_add16( p_buf, p_out[i] );
    free( p_out );

    var_buffer_add16( p_buf, 0 );
}

void var_buffer_free( var_buffer_t *p_buf )
{
    free( p_buf->p_data );
    p_buf->i_data = 0;
    p_buf->i_size = 0;
}

void var_buffer_initread( var_buffer_t *p_buf, void *p_data, int i_data )
{
    p_buf->i_size = i_data;
    p_buf->i_data = 0;
    p_buf->p_data = p_data;
}

uint8_t var_buffer_get8 ( var_buffer_t *p_buf )
{
    uint8_t  i_byte;
    if( p_buf->i_data >= p_buf->i_size )
    {
        return( 0 );
    }
    i_byte = p_buf->p_data[p_buf->i_data];
    p_buf->i_data++;
    return( i_byte );
}

uint16_t var_buffer_get16( var_buffer_t *p_buf )
{
    uint16_t i_b1, i_b2;

    i_b1 = var_buffer_get8( p_buf );
    i_b2 = var_buffer_get8( p_buf );

    return( i_b1 + ( i_b2 << 8 ) );

}

uint32_t var_buffer_get32( var_buffer_t *p_buf )
{
    uint32_t i_w1, i_w2;

    i_w1 = var_buffer_get16( p_buf );
    i_w2 = var_buffer_get16( p_buf );

    return( i_w1 + ( i_w2 << 16 ) );
}

uint64_t var_buffer_get64( var_buffer_t *p_buf )
{
    uint64_t i_dw1, i_dw2;

    i_dw1 = var_buffer_get32( p_buf );
    i_dw2 = var_buffer_get32( p_buf );

    return( i_dw1 + ( i_dw2 << 32 ) );
}

int var_buffer_getmemory ( var_buffer_t *p_buf, void *p_mem, int64_t i_mem )
{
    int i_copy;

    i_copy = __MIN( i_mem, p_buf->i_size - p_buf->i_data );
    if( i_copy > 0 && p_mem != NULL)
    {
        memcpy( p_mem, p_buf->p_data + p_buf->i_data , i_copy );
    }
    if( i_copy < 0 )
    {
        i_copy = 0;
    }
    p_buf->i_data += i_copy;
    return( i_copy );
}

int var_buffer_readempty( var_buffer_t *p_buf )
{
    return( ( p_buf->i_data >= p_buf->i_size ) ? 1 : 0 );
}

void var_buffer_getguid( var_buffer_t *p_buf, guid_t *p_guid )
{
    int i;

    p_guid->Data1 = var_buffer_get32( p_buf );
    p_guid->Data2 = var_buffer_get16( p_buf );
    p_guid->Data3 = var_buffer_get16( p_buf );

    for( i = 0; i < 8; i++ )
    {
        p_guid->Data4[i] = var_buffer_get8( p_buf );
    }
}
