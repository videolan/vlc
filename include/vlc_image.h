/*****************************************************************************
 * vlc_image.h : wrapper for image reading/writing facilities
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifndef VLC_IMAGE_H
#define VLC_IMAGE_H 1

# include <vlc_picture.h>
# include <vlc_picture_fifo.h>

/**
 * \file
 * This file defines functions and structures for image conversions in vlc
 */

# ifdef __cplusplus
extern "C" {
# endif

struct image_handler_t
{
    picture_t * (*pf_read)      ( image_handler_t *, block_t *,
                                  const video_format_t *, video_format_t * );
    picture_t * (*pf_read_url)  ( image_handler_t *, const char *,
                                  video_format_t *, video_format_t * );
    block_t * (*pf_write)       ( image_handler_t *, picture_t *,
                                  const video_format_t *, const video_format_t * );
    int (*pf_write_url)         ( image_handler_t *, picture_t *,
                                  const video_format_t *, video_format_t *,
                                  const char * );

    picture_t * (*pf_convert)   ( image_handler_t *, picture_t *,
                                  const video_format_t *, video_format_t * );

    /* Private properties */
    vlc_object_t *p_parent;
    decoder_t *p_dec;
    encoder_t *p_enc;
    filter_t  *p_filter;

    picture_fifo_t *outfifo;
};

VLC_API image_handler_t * image_HandlerCreate( vlc_object_t * ) VLC_USED;
#define image_HandlerCreate( a ) image_HandlerCreate( VLC_OBJECT(a) )
VLC_API void image_HandlerDelete( image_handler_t * );

#define image_Read( a, b, c, d ) a->pf_read( a, b, c, d )
#define image_ReadUrl( a, b, c, d ) a->pf_read_url( a, b, c, d )
#define image_Write( a, b, c, d ) a->pf_write( a, b, c, d )
#define image_WriteUrl( a, b, c, d, e ) a->pf_write_url( a, b, c, d, e )
#define image_Convert( a, b, c, d ) a->pf_convert( a, b, c, d )

VLC_API vlc_fourcc_t image_Type2Fourcc( const char *psz_name );
VLC_API vlc_fourcc_t image_Ext2Fourcc( const char *psz_name );
VLC_API vlc_fourcc_t image_Mime2Fourcc( const char *psz_mime );

# ifdef __cplusplus
}
# endif

#endif /* _VLC_IMAGE_H */
