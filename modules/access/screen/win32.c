/*****************************************************************************
 * win32.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
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

#ifndef CAPTUREBLT
#   define CAPTUREBLT (DWORD)0x40000000 /* Include layered windows */
#endif

struct screen_data_t
{
    HDC hdc_src;
    HDC hdc_dst;
    BITMAPINFO bmi;
    HGDIOBJ hgdi_backup;

    int i_fragment_size;
    int i_fragment;
    block_t *p_block;
};

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    int i_chroma, i_bits_per_pixel;
    vlc_value_t val;

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
    p_data->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    p_data->bmi.bmiHeader.biWidth = p_sys->fmt.video.i_width;
    p_data->bmi.bmiHeader.biHeight = - p_sys->fmt.video.i_height;
    p_data->bmi.bmiHeader.biPlanes = 1;
    p_data->bmi.bmiHeader.biBitCount = p_sys->fmt.video.i_bits_per_pixel;
    p_data->bmi.bmiHeader.biCompression = BI_RGB;
    p_data->bmi.bmiHeader.biSizeImage = 0;
    p_data->bmi.bmiHeader.biXPelsPerMeter =
        p_data->bmi.bmiHeader.biYPelsPerMeter = 0;
    p_data->bmi.bmiHeader.biClrUsed = 0;
    p_data->bmi.bmiHeader.biClrImportant = 0;

    if( i_chroma == VLC_FOURCC('R','V','2','4') )
    {
        /* This is in BGR format */
        p_sys->fmt.video.i_bmask = 0x00ff0000;
        p_sys->fmt.video.i_gmask = 0x0000ff00;
        p_sys->fmt.video.i_rmask = 0x000000ff;
    }

    var_Create( p_demux, "screen-fragment-size",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "screen-fragment-size", &val );
    p_data->i_fragment_size =
        val.i_int > 0 ? val.i_int : p_sys->fmt.video.i_height;
    p_data->i_fragment_size =
        val.i_int > p_sys->fmt.video.i_height ? p_sys->fmt.video.i_height :
        p_data->i_fragment_size;
    p_sys->f_fps *= (p_sys->fmt.video.i_height/p_data->i_fragment_size);
    p_sys->i_incr = 1000000 / p_sys->f_fps;
    p_data->i_fragment = 0;
    p_data->p_block = 0;

    return VLC_SUCCESS;
}

int screen_CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;

    if( p_data->p_block ) block_Release( p_data->p_block );

    if( p_data->hgdi_backup)
        SelectObject( p_data->hdc_dst, p_data->hgdi_backup );

    DeleteDC( p_data->hdc_dst );
    ReleaseDC( 0, p_data->hdc_src );
    free( p_data );

    return VLC_SUCCESS;
}

struct block_sys_t
{
    HBITMAP hbmp;
};

static void CaptureBlockRelease( block_t *p_block )
{
    DeleteObject( p_block->p_sys->hbmp );
    free( p_block );
}

static block_t *CaptureBlockNew( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    block_t *p_block;
    void *p_buffer;
    int i_buffer;
    HBITMAP hbmp;

    /* Create the bitmap storage space */
    hbmp = CreateDIBSection( p_data->hdc_dst, &p_data->bmi, DIB_RGB_COLORS,
                             &p_buffer, NULL, 0 );
    if( !hbmp || !p_buffer )
    {
        msg_Err( p_demux, "cannot create bitmap" );
        if( hbmp ) DeleteObject( hbmp );
        return NULL;
    }

    /* Select the bitmap into the compatible DC */
    if( !p_data->hgdi_backup )
        p_data->hgdi_backup = SelectObject( p_data->hdc_dst, hbmp );
    else
        SelectObject( p_data->hdc_dst, hbmp );

    if( !p_data->hgdi_backup )
    {
        msg_Err( p_demux, "cannot select bitmap" );
        DeleteObject( hbmp );
        return NULL;
    }

    /* Build block */
    if( !(p_block = malloc( sizeof( block_t ) + sizeof( block_sys_t ) )) )
    {
        DeleteObject( hbmp );
        return NULL;
    }
    memset( p_block, 0, sizeof( block_t ) );
    p_block->p_sys = (block_sys_t *)( (uint8_t *)p_block + sizeof( block_t ) );

    /* Fill all fields */
    i_buffer = (p_sys->fmt.video.i_bits_per_pixel + 7) / 8 *
        p_sys->fmt.video.i_width * p_sys->fmt.video.i_height;
    p_block->p_next         = NULL;
    p_block->i_buffer       = i_buffer;
    p_block->p_buffer       = p_buffer;
    p_block->pf_release     = CaptureBlockRelease;
    p_block->p_manager      = VLC_OBJECT( p_demux->p_vlc );
    p_block->p_sys->hbmp    = hbmp;

    return p_block;
}

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;

    if( !p_data->i_fragment )
    {
        if( !( p_data->p_block = CaptureBlockNew( p_demux ) ) )
        {
            msg_Warn( p_demux, "cannot get block" );
            return 0;
        }
    }

    if( !BitBlt( p_data->hdc_dst, 0, p_data->i_fragment *
                 p_data->i_fragment_size,
                 p_sys->fmt.video.i_width, p_data->i_fragment_size,
                 p_data->hdc_src, 0, p_data->i_fragment *
                 p_data->i_fragment_size,
                 IS_WINNT ? SRCCOPY | CAPTUREBLT : SRCCOPY ) )
    {
        msg_Err( p_demux, "error during BitBlt()" );
        return NULL;
    }

    p_data->i_fragment++;

    if( !( p_data->i_fragment %
           (p_sys->fmt.video.i_height/p_data->i_fragment_size) ) )
    {
        block_t *p_block = p_data->p_block;
        p_data->i_fragment = 0;
        p_data->p_block = 0;
        return p_block;
    }

    return NULL;
}
