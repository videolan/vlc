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
    return VLC_EGENERIC;
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
    return NULL;
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
