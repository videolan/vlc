/*****************************************************************************
 * win32.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "screen.h"

struct screen_data_t
{
    HDC hdc_src;
    HDC hdc_dst;
    HBITMAP hbmp;
    HGDIOBJ hgdi_backup;
    uint8_t *p_buffer;
};

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    int i_chroma, i_bits_per_pixel;

    BITMAPINFO bmi;

    p_sys->p_data = p_data = malloc( sizeof( screen_data_t ) );

    /* Get the device context for the whole screen */
    p_data->hdc_src = CreateDC( "DISPLAY", NULL, NULL, NULL );
    if( !p_data->hdc_src )
    {
        msg_Err( p_demux, "cannot get device context" );
        return VLC_EGENERIC;
    }

    p_data->hdc_dst = CreateCompatibleDC( p_data->hdc_src );
    if( !p_data->hdc_dst )
    {
        msg_Err( p_demux, "cannot get compat device context" );
        ReleaseDC( 0, p_data->hdc_src );
        return VLC_EGENERIC;
    }

    i_bits_per_pixel = GetDeviceCaps( p_data->hdc_src, BITSPIXEL );
    switch( i_bits_per_pixel )
    {
    case 8: /* FIXME: set the palette */
        i_chroma = VLC_FOURCC('R','G','B','2'); break;
    case 15:
        i_chroma = VLC_FOURCC('R','V','1','5'); break;
    case 16:
        i_chroma = VLC_FOURCC('R','V','1','6'); break;
    case 24:
        i_chroma = VLC_FOURCC('R','V','2','4'); break;
    case 32:
        i_chroma = VLC_FOURCC('R','V','3','2'); break;
    default:
        msg_Err( p_demux, "unknown screen depth %i",
                 p_sys->fmt.video.i_bits_per_pixel );
        ReleaseDC( 0, p_data->hdc_src );
        ReleaseDC( 0, p_data->hdc_dst );
        return VLC_EGENERIC;
    }

#if 1 /* For now we force RV24 because of chroma inversion in the other cases*/
    i_chroma = VLC_FOURCC('R','V','2','4');
    i_bits_per_pixel = 24;
#endif

    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_width  = GetDeviceCaps( p_data->hdc_src, HORZRES );
    p_sys->fmt.video.i_height = GetDeviceCaps( p_data->hdc_src, VERTRES );
    p_sys->fmt.video.i_bits_per_pixel = i_bits_per_pixel;

    /* Create the bitmap info header */
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = p_sys->fmt.video.i_width;
    bmi.bmiHeader.biHeight = - p_sys->fmt.video.i_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = p_sys->fmt.video.i_bits_per_pixel;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;
    bmi.bmiHeader.biXPelsPerMeter =
        bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;

    /* Create the bitmap storage space */
    p_data->hbmp = CreateDIBSection( p_data->hdc_dst, (BITMAPINFO *)&bmi,
        DIB_RGB_COLORS, (void **)&p_data->p_buffer, NULL, 0 );
    if( !p_data->hbmp || !p_data->p_buffer )
    {
        msg_Err( p_demux, "cannot create bitmap" );
        if( p_data->hbmp ) DeleteObject( p_data->hbmp );
        ReleaseDC( 0, p_data->hdc_src );
        DeleteDC( p_data->hdc_dst );
        return VLC_EGENERIC;
    }

    /* Select the bitmap into the compatible DC */
    p_data->hgdi_backup = SelectObject( p_data->hdc_dst, p_data->hbmp );
    if( !p_data->hgdi_backup )
    {
        msg_Err( p_demux, "cannot select bitmap" );
    }

    return VLC_SUCCESS;
}

int screen_CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;

    SelectObject( p_data->hdc_dst, p_data->hgdi_backup );
    DeleteObject( p_data->hbmp );
    DeleteDC( p_data->hdc_dst );
    ReleaseDC( 0, p_data->hdc_src );
    free( p_data );

    return VLC_SUCCESS;
}

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    block_t *p_block;
    int i_size;

    if( !BitBlt( p_data->hdc_dst, 0, 0,
                 p_sys->fmt.video.i_width, p_sys->fmt.video.i_height,
                 p_data->hdc_src, 0, 0, SRCCOPY ) )
    {
        msg_Err( p_demux, "error during BitBlt()" );
        return NULL;
    }

    i_size = (p_sys->fmt.video.i_bits_per_pixel + 7) / 8 *
        p_sys->fmt.video.i_width * p_sys->fmt.video.i_height;

    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    memcpy( p_block->p_buffer, p_data->p_buffer, i_size );

    return p_block;
}

