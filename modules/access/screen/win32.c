/*****************************************************************************
 * win32.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004-2011 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "screen.h"

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

    p_sys->p_data = p_data = calloc( 1, sizeof( screen_data_t ) );
    if( !p_data )
        return VLC_ENOMEM;

    /* Get the device context for the whole screen */
    p_data->hdc_src = CreateDC( TEXT("DISPLAY"), NULL, NULL, NULL );
    if( !p_data->hdc_src )
    {
        msg_Err( p_demux, "cannot get device context" );
        free( p_data );
        return VLC_EGENERIC;
    }

    p_data->hdc_dst = CreateCompatibleDC( p_data->hdc_src );
    if( !p_data->hdc_dst )
    {
        msg_Err( p_demux, "cannot get compat device context" );
        free( p_data );
        ReleaseDC( 0, p_data->hdc_src );
        return VLC_EGENERIC;
    }

    i_bits_per_pixel = GetDeviceCaps( p_data->hdc_src, BITSPIXEL );
    switch( i_bits_per_pixel )
    {
    case 8: /* FIXME: set the palette */
        i_chroma = VLC_CODEC_RGB8; break;
    case 15:
    case 16:    /* Yes it is really 15 bits (when using BI_RGB) */
        i_chroma = VLC_CODEC_RGB15; break;
    case 24:
        i_chroma = VLC_CODEC_RGB24; break;
    case 32:
        i_chroma = VLC_CODEC_RGB32; break;
    default:
        msg_Err( p_demux, "unknown screen depth %i", i_bits_per_pixel );
        DeleteDC( p_data->hdc_dst );
        ReleaseDC( 0, p_data->hdc_src );
        free( p_data );
        return VLC_EGENERIC;
    }

    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_visible_width  =
    p_sys->fmt.video.i_width          = GetDeviceCaps( p_data->hdc_src, HORZRES );
    p_sys->fmt.video.i_visible_height =
    p_sys->fmt.video.i_height         = GetDeviceCaps( p_data->hdc_src, VERTRES );
    p_sys->fmt.video.i_bits_per_pixel = i_bits_per_pixel;
    p_sys->fmt.video.i_chroma         = i_chroma;

    switch( i_chroma )
    {
    case VLC_CODEC_RGB15:
        p_sys->fmt.video.i_rmask = 0x7c00;
        p_sys->fmt.video.i_gmask = 0x03e0;
        p_sys->fmt.video.i_bmask = 0x001f;
        break;
    case VLC_CODEC_RGB24:
        p_sys->fmt.video.i_rmask = 0x00ff0000;
        p_sys->fmt.video.i_gmask = 0x0000ff00;
        p_sys->fmt.video.i_bmask = 0x000000ff;
        break;
    case VLC_CODEC_RGB32:
        p_sys->fmt.video.i_rmask = 0x00ff0000;
        p_sys->fmt.video.i_gmask = 0x0000ff00;
        p_sys->fmt.video.i_bmask = 0x000000ff;
        break;
    default:
        msg_Warn( p_demux, "Unknown RGB masks" );
        break;
    }

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
    block_t self;
    HBITMAP hbmp;
};

static void CaptureBlockRelease( block_t *p_block )
{
    DeleteObject( ((struct block_sys_t *)p_block)->hbmp );
    free( p_block );
}

static block_t *CaptureBlockNew( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;
    struct block_sys_t *p_block;
    void *p_buffer;
    int i_buffer;
    HBITMAP hbmp;

    if( p_data->bmi.bmiHeader.biSize == 0 )
    {
        int i_val;
        /* Create the bitmap info header */
        p_data->bmi.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        p_data->bmi.bmiHeader.biWidth         = p_sys->fmt.video.i_width;
        p_data->bmi.bmiHeader.biHeight        = - p_sys->fmt.video.i_height;
        p_data->bmi.bmiHeader.biPlanes        = 1;
        p_data->bmi.bmiHeader.biBitCount      = p_sys->fmt.video.i_bits_per_pixel;
        p_data->bmi.bmiHeader.biCompression   = BI_RGB;
        p_data->bmi.bmiHeader.biSizeImage     = 0;
        p_data->bmi.bmiHeader.biXPelsPerMeter = 0;
        p_data->bmi.bmiHeader.biYPelsPerMeter = 0;
        p_data->bmi.bmiHeader.biClrUsed       = 0;
        p_data->bmi.bmiHeader.biClrImportant  = 0;

        i_val = var_CreateGetInteger( p_demux, "screen-fragment-size" );
        p_data->i_fragment_size = i_val > 0 ? i_val : (int)p_sys->fmt.video.i_height;
        p_data->i_fragment_size = i_val > (int)p_sys->fmt.video.i_height ?
                                            (int)p_sys->fmt.video.i_height :
                                            p_data->i_fragment_size;
        p_sys->f_fps *= (p_sys->fmt.video.i_height/p_data->i_fragment_size);
        p_sys->i_incr = 1000000 / p_sys->f_fps;
        p_data->i_fragment = 0;
        p_data->p_block = 0;
    }


    /* Create the bitmap storage space */
    hbmp = CreateDIBSection( p_data->hdc_dst, &p_data->bmi, DIB_RGB_COLORS,
                             &p_buffer, NULL, 0 );
    if( !hbmp || !p_buffer )
    {
        msg_Err( p_demux, "cannot create bitmap" );
        goto error;
    }

    /* Select the bitmap into the compatible DC */
    if( !p_data->hgdi_backup )
        p_data->hgdi_backup = SelectObject( p_data->hdc_dst, hbmp );
    else
        SelectObject( p_data->hdc_dst, hbmp );

    if( !p_data->hgdi_backup )
    {
        msg_Err( p_demux, "cannot select bitmap" );
        goto error;
    }

    /* Build block */
    if( !(p_block = malloc( sizeof( block_t ) + sizeof( struct block_sys_t ) )) )
        goto error;

    /* Fill all fields */
    i_buffer = (p_sys->fmt.video.i_bits_per_pixel + 7) / 8 *
        p_sys->fmt.video.i_width * p_sys->fmt.video.i_height;
    block_Init( &p_block->self, p_buffer, i_buffer );
    p_block->self.pf_release = CaptureBlockRelease;
    p_block->hbmp            = hbmp;

    return &p_block->self;

error:
    if( hbmp ) DeleteObject( hbmp );
    return NULL;
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
            return NULL;
        }
    }

    if( p_sys->b_follow_mouse )
    {
        POINT pos;
        GetCursorPos( &pos );
        FollowMouse( p_sys, pos.x, pos.y );
    }

    if( !BitBlt( p_data->hdc_dst, 0,
                 p_data->i_fragment * p_data->i_fragment_size,
                 p_sys->fmt.video.i_width, p_data->i_fragment_size,
                 p_data->hdc_src, p_sys->i_left, p_sys->i_top +
                 p_data->i_fragment * p_data->i_fragment_size,
                 SRCCOPY | CAPTUREBLT ) )
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

        if( p_sys->p_mouse )
        {
            POINT pos;
            GetCursorPos( &pos );
            RenderCursor( p_demux, pos.x, pos.y,
                          p_block->p_buffer );
        }

        return p_block;
    }

    return NULL;
}
