/*****************************************************************************
 * picture.c:  libvlc API picture management
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>

#include <vlc_atomic.h>
#include <vlc_picture.h>
#include <vlc_block.h>
#include <vlc_fs.h>

#include "picture_internal.h"

struct libvlc_picture_t
{
    vlc_atomic_rc_t rc;
    libvlc_picture_type_t type;
    block_t* converted;
    video_format_t fmt;
    libvlc_time_t time;
};

libvlc_picture_t* libvlc_picture_new( vlc_object_t* p_obj, picture_t* input,
                                      libvlc_picture_type_t type,
                                      unsigned int width, unsigned int height,
                                      bool crop )
{
    libvlc_picture_t *pic = malloc( sizeof( *pic ) );
    if ( unlikely( pic == NULL ) )
        return NULL;
    vlc_atomic_rc_init( &pic->rc );
    pic->type = type;
    pic->time = MS_FROM_VLC_TICK( input->date );
    vlc_fourcc_t format;
    switch ( type )
    {
        case libvlc_picture_Argb:
            format = VLC_CODEC_ARGB;
            break;
        case libvlc_picture_Jpg:
            format = VLC_CODEC_JPEG;
            break;
        case libvlc_picture_Png:
            format = VLC_CODEC_PNG;
            break;
        default:
            vlc_assert_unreachable();
    }
    if ( picture_Export( p_obj, &pic->converted, &pic->fmt,
                         input, format, width, height, crop ) != VLC_SUCCESS )
    {
        free( pic );
        return NULL;
    }

    return pic;
}

void libvlc_picture_retain( libvlc_picture_t* pic )
{
    vlc_atomic_rc_inc( &pic->rc );
}

void libvlc_picture_release( libvlc_picture_t* pic )
{
    if ( vlc_atomic_rc_dec( &pic->rc ) == false )
        return;
    video_format_Clean( &pic->fmt );
    if ( pic->converted )
        block_Release( pic->converted );
    free( pic );
}

int libvlc_picture_save( const libvlc_picture_t* pic, const char* path )
{
    FILE* file = vlc_fopen( path, "wb" );
    if ( !file )
        return -1;
    size_t res = fwrite( pic->converted->p_buffer,
                         pic->converted->i_buffer, 1, file );
    fclose( file );
    return res == 1 ? 0 : -1;
}

const unsigned char* libvlc_picture_get_buffer( const libvlc_picture_t* pic,
                                                size_t *size )
{
    assert( size != NULL );
    *size = pic->converted->i_buffer;
    return pic->converted->p_buffer;
}

libvlc_picture_type_t libvlc_picture_type( const libvlc_picture_t* pic )
{
    return pic->type;
}

unsigned int libvlc_picture_get_stride( const libvlc_picture_t *pic )
{
    assert( pic->type == libvlc_picture_Argb );
    return pic->fmt.i_width * pic->fmt.i_bits_per_pixel / 8;
}

unsigned int libvlc_picture_get_width( const libvlc_picture_t* pic )
{
    return pic->fmt.i_visible_width;
}

unsigned int libvlc_picture_get_height( const libvlc_picture_t* pic )
{
    return pic->fmt.i_visible_height;
}

libvlc_time_t libvlc_picture_get_time( const libvlc_picture_t* pic )
{
    return pic->time;
}
