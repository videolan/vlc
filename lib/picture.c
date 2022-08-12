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
#include "libvlc_internal.h"

#include <vlc_atomic.h>
#include <vlc_picture.h>
#include <vlc_block.h>
#include <vlc_image.h>
#include <vlc_input.h>
#include <vlc_fs.h>

#include "picture_internal.h"

struct libvlc_picture_t
{
    vlc_atomic_rc_t rc;
    libvlc_picture_type_t type;
    block_t* converted;
    video_format_t fmt;
    libvlc_time_t time;
    input_attachment_t* attachment;
};

struct libvlc_picture_list_t
{
    size_t count;
    libvlc_picture_t* pictures[];
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
    pic->time = libvlc_time_from_vlc_tick( input->date );
    pic->attachment = NULL;
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

static void libvlc_picture_block_release( block_t* block )
{
    free( block );
}

static const struct vlc_block_callbacks block_cbs =
{
    libvlc_picture_block_release,
};

static libvlc_picture_t* libvlc_picture_from_attachment( input_attachment_t* attachment )
{
    vlc_fourcc_t fcc = image_Mime2Fourcc( attachment->psz_mime );
    if ( fcc != VLC_CODEC_PNG && fcc != VLC_CODEC_JPEG )
        return NULL;
    libvlc_picture_t *pic = malloc( sizeof( *pic ) );
    if ( unlikely( pic == NULL ) )
        return NULL;
    pic->converted = malloc( sizeof( *pic->converted ) );
    if ( unlikely( pic->converted == NULL ) )
    {
        free(pic);
        return NULL;
    }
    vlc_atomic_rc_init( &pic->rc );
    pic->attachment = vlc_input_attachment_Hold( attachment );
    pic->time = VLC_TICK_INVALID;
    block_Init( pic->converted, &block_cbs, attachment->p_data,
                attachment->i_data);
    video_format_Init( &pic->fmt, fcc );
    switch ( fcc )
    {
    case VLC_CODEC_PNG:
        pic->type = libvlc_picture_Png;
        break;
    case VLC_CODEC_JPEG:
        pic->type = libvlc_picture_Jpg;
        break;
    default:
        vlc_assert_unreachable();
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
    if ( pic->attachment )
        vlc_input_attachment_Release( pic->attachment );
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

libvlc_picture_list_t* libvlc_picture_list_from_attachments( input_attachment_t** attachments,
                                                             size_t nb_attachments )
{
    size_t size = 0;
    libvlc_picture_list_t* list;
    if ( mul_overflow( nb_attachments, sizeof( libvlc_picture_t* ), &size ) )
        return NULL;
    if ( add_overflow( size, sizeof( *list ), &size ) )
        return NULL;

    list = malloc( size );
    if ( !list )
        return NULL;
    list->count = 0;
    for ( size_t i = 0; i < nb_attachments; ++i )
    {
        input_attachment_t* a = attachments[i];
        libvlc_picture_t *pic = libvlc_picture_from_attachment( a );
        if( !pic )
            continue;
        list->pictures[list->count] = pic;
        list->count++;
    }
    return list;
}

size_t libvlc_picture_list_count( const libvlc_picture_list_t* list )
{
    assert( list );
    return list->count;
}

libvlc_picture_t* libvlc_picture_list_at( const libvlc_picture_list_t* list,
                                          size_t index )
{
    assert( list );
    return list->pictures[index];
}

void libvlc_picture_list_destroy( libvlc_picture_list_t* list )
{
    if ( !list )
        return;
    for ( size_t i = 0; i < list->count; ++i )
        libvlc_picture_release( list->pictures[i] );
    free( list );
}
