/*****************************************************************************
 * beos.cpp: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Eric Petit <titer@m0k.org>
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


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <Screen.h>
#include <Bitmap.h>

extern "C"
{

#include "screen.h"

struct screen_data_t
{
    BScreen * p_screen;
    BBitmap * p_bitmap;
};

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t   *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    BRect          rect;
    int            i_chroma;
    int            i_bits_per_pixel;

    p_sys->p_data = p_data =
        (screen_data_t *)malloc( sizeof( screen_data_t ) );

    p_data->p_screen = new BScreen();
    rect = p_data->p_screen->Frame();

    p_data->p_bitmap = new BBitmap( rect, p_data->p_screen->ColorSpace() );

    switch( p_data->p_screen->ColorSpace() )
    {
        case B_RGB32:
            i_chroma = VLC_FOURCC('R','V','3','2');
            i_bits_per_pixel = 32;
            break;
        case B_RGB16:
            i_chroma = VLC_FOURCC('R','V','1','6');
            i_bits_per_pixel = 16;
            break;
        default:
            msg_Err( p_demux, "screen depth %i unsupported",
                     p_data->p_screen->ColorSpace() );
            delete p_data->p_bitmap;
            delete p_data->p_screen;
            free( p_data );
            return VLC_EGENERIC;
    }
    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_width  = (int)rect.Width();
    p_sys->fmt.video.i_height = (int)rect.Height();
    p_sys->fmt.video.i_bits_per_pixel = i_bits_per_pixel;

    return VLC_SUCCESS;
}

int screen_CloseCapture( demux_t *p_demux )
{
    demux_sys_t   *p_sys  = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;

    delete p_data->p_bitmap;
    delete p_data->p_screen;
    free( p_data );

    return VLC_SUCCESS;
}

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t   *p_sys  = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    block_t       *p_block;

    p_block = block_New( p_demux, p_sys->fmt.video.i_width *
                         p_sys->fmt.video.i_height *
                         p_sys->fmt.video.i_bits_per_pixel / 8 );

    p_data->p_screen->ReadBitmap( p_data->p_bitmap );

    for( unsigned i = 0; i < p_sys->fmt.video.i_height; i++ )
    {
        memcpy( p_block->p_buffer + i * p_sys->fmt.video.i_width *
                    p_sys->fmt.video.i_bits_per_pixel / 8,
                (uint8_t *) p_data->p_bitmap->Bits() +
                    i * p_data->p_bitmap->BytesPerRow(),
                p_sys->fmt.video.i_width *
                    p_sys->fmt.video.i_bits_per_pixel / 8 );
    }
    return p_block;
}

} /* extern "C" */
