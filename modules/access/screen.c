/*****************************************************************************
 * screen.c: Screen capture module.
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

#ifndef WIN32
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for screen capture " \
    "streams. This value should be set in millisecond units." )
#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "Allows you to set the desired frame rate for the capture." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Screen Input") );

    add_integer( "screen-caching", DEFAULT_PTS_DELAY / 1000, NULL,
        CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_float( "screen-fps", 5, NULL, FPS_TEXT, FPS_LONGTEXT, VLC_TRUE );

    set_capability( "access_demux", 0 );
    add_shortcut( "screen" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    es_format_t fmt;
    es_out_id_t *es;

    float f_fps;
    mtime_t i_next_date;
    int i_incr;

#ifndef WIN32
    Display *p_display;

#else
    HDC hdc_src;
    HDC hdc_dst;
    HBITMAP hbmp;
    HGDIOBJ hgdi_backup;
    uint8_t *p_buffer;
#endif
};

static int Control( demux_t *, int, va_list );
static int Demux  ( demux_t * );

static int InitCapture ( demux_t * );
static int CloseCapture( demux_t * );
static block_t *Capture( demux_t * );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    vlc_value_t val;

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    if( InitCapture( p_demux ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "screen width: %i, height: %i, depth: %i",
             p_sys->fmt.video.i_width, p_sys->fmt.video.i_height,
             p_sys->fmt.video.i_bits_per_pixel );

    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );

    /* Update default_pts to a suitable value for screen access */
    var_Create( p_demux, "screen-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_demux, "screen-fps", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
    var_Get( p_demux, "screen-fps", &val );
    p_sys->f_fps = val.f_float;
    p_sys->i_incr = 1000000 / val.f_float;
    p_sys->i_next_date = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    CloseCapture( p_demux );
    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;

    if( !p_sys->i_next_date ) p_sys->i_next_date = mdate();

    /* Frame skipping if necessary */
    while( mdate() >= p_sys->i_next_date + p_sys->i_incr )
        p_sys->i_next_date += p_sys->i_incr;

    mwait( p_sys->i_next_date );
    p_block = Capture( p_demux );
    if( !p_block ) return 0;

    p_block->i_dts = p_block->i_pts = p_sys->i_next_date;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->es, p_block );

    p_sys->i_next_date += p_sys->i_incr;

    return 1;
}

/*****************************************************************************
 * Platform dependant capture functions
 *****************************************************************************/
#ifdef WIN32
static int InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chroma, i_bits_per_pixel;

    BITMAPINFO bmi;

    /* Get the device context for the whole screen */
    p_sys->hdc_src = CreateDC( "DISPLAY", NULL, NULL, NULL );
    if( !p_sys->hdc_src )
    {
        msg_Err( p_demux, "cannot get device context" );
        return VLC_EGENERIC;
    }

    p_sys->hdc_dst = CreateCompatibleDC( p_sys->hdc_src );
    if( !p_sys->hdc_dst )
    {
        msg_Err( p_demux, "cannot get compat device context" );
        ReleaseDC( 0, p_sys->hdc_src );
        return VLC_EGENERIC;
    }

    i_bits_per_pixel = GetDeviceCaps( p_sys->hdc_src, BITSPIXEL );
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
        ReleaseDC( 0, p_sys->hdc_src );
        ReleaseDC( 0, p_sys->hdc_dst );
        return VLC_EGENERIC;
    }

#if 1 /* For now we force RV24 because of chroma inversion in the other cases*/
    i_chroma = VLC_FOURCC('R','V','2','4');
    i_bits_per_pixel = 24;
#endif

    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_width  = GetDeviceCaps( p_sys->hdc_src, HORZRES );
    p_sys->fmt.video.i_height = GetDeviceCaps( p_sys->hdc_src, VERTRES );
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
    p_sys->hbmp = CreateDIBSection( p_sys->hdc_dst, (BITMAPINFO *)&bmi,
        DIB_RGB_COLORS, (void **)&p_sys->p_buffer, NULL, 0 );
    if( !p_sys->hbmp || !p_sys->p_buffer )
    {
        msg_Err( p_demux, "cannot create bitmap" );
        if( p_sys->hbmp ) DeleteObject( p_sys->hbmp );
        ReleaseDC( 0, p_sys->hdc_src );
        DeleteDC( p_sys->hdc_dst );
        return VLC_EGENERIC;
    }

    /* Select the bitmap into the compatible DC */
    p_sys->hgdi_backup = SelectObject( p_sys->hdc_dst, p_sys->hbmp );
    if( !p_sys->hgdi_backup )
    {
        msg_Err( p_demux, "cannot select bitmap" );
    }

    return VLC_SUCCESS;
}
#else

static int InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    XWindowAttributes win_info;
    int i_chroma;

    /* Open the display */
    p_sys->p_display = XOpenDisplay( NULL );
    if( !p_sys->p_display )
    {
        msg_Err( p_demux, "cannot open display" );
        return VLC_EGENERIC;
    }

    /* Get the parameters of the root window */
    if( !XGetWindowAttributes( p_sys->p_display,
                               DefaultRootWindow( p_sys->p_display ),
                               &win_info ) )
    {
        msg_Err( p_demux, "can't get root window attributes" );
        XCloseDisplay( p_sys->p_display );
        return VLC_EGENERIC;
    }

    switch( win_info.depth )
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
        msg_Err( p_demux, "unknown screen depth %i", win_info.depth );
        XCloseDisplay( p_sys->p_display );
        return VLC_EGENERIC;
    }

    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_width  = win_info.width;
    p_sys->fmt.video.i_height = win_info.height;
    p_sys->fmt.video.i_bits_per_pixel = win_info.depth;

#if 0
    win_info.visual->red_mask;
    win_info.visual->green_mask;
    win_info.visual->blue_mask;
    win_info.visual->bits_per_rgb;
#endif

    return VLC_SUCCESS;
}
#endif

#ifdef WIN32
static int CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    SelectObject( p_sys->hdc_dst, p_sys->hgdi_backup );
    DeleteObject( p_sys->hbmp );
    DeleteDC( p_sys->hdc_dst );
    ReleaseDC( 0, p_sys->hdc_src );
    return VLC_SUCCESS;
}

#else
static int CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    XCloseDisplay( p_sys->p_display );
    return VLC_SUCCESS;
}
#endif

#ifdef WIN32
static block_t *Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;
    int i_size;

    if( !BitBlt( p_sys->hdc_dst, 0, 0,
                 p_sys->fmt.video.i_width, p_sys->fmt.video.i_height,
                 p_sys->hdc_src, 0, 0, SRCCOPY ) )
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

    memcpy( p_block->p_buffer, p_sys->p_buffer, i_size );

    return p_block;
}

#else
static block_t *Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;
    XImage *image;
    int i_size;

    image = XGetImage( p_sys->p_display, DefaultRootWindow( p_sys->p_display ),
                       0, 0, p_sys->fmt.video.i_width,
                       p_sys->fmt.video.i_height, AllPlanes, ZPixmap );

    if( !image )
    {
        msg_Warn( p_demux, "cannot get image" );
        return 0;
    }

    i_size = image->bytes_per_line * image->height;

    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "cannot get block" );
        XDestroyImage( image );
        return 0;
    }

    memcpy( p_block->p_buffer, image->data, i_size );

    XDestroyImage( image );

    return p_block;
}
#endif

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    vlc_bool_t *pb;
    int64_t *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
            *pb = VLC_FALSE;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "screen-caching" ) *1000;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}
