/*****************************************************************************
 * x11.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "screen.h"

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    Display *p_display;
    char *psz_display = var_CreateGetNonEmptyString( p_demux, "x11-display" );
    XWindowAttributes win_info;
    int i_chroma;

    /* Open the display */
    p_display = XOpenDisplay( psz_display );
    free( psz_display );
    if( !p_display )
    {
        msg_Err( p_demux, "cannot open display" );
        return VLC_EGENERIC;
    }
    p_sys->p_data = (void *)p_display;

    /* Get the parameters of the root window */
    if( !XGetWindowAttributes( p_display,
                               DefaultRootWindow( p_display ),
                               &win_info ) )
    {
        msg_Err( p_demux, "can't get root window attributes" );
        XCloseDisplay( p_display );
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
    case 32:
        i_chroma = VLC_FOURCC('R','V','3','2');
        win_info.depth = 32;
        break;
    default:
        msg_Err( p_demux, "unknown screen depth %i", win_info.depth );
        XCloseDisplay( p_display );
        return VLC_EGENERIC;
    }

    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_visible_width =
    p_sys->fmt.video.i_width  = win_info.width;
    p_sys->fmt.video.i_visible_height =
    p_sys->fmt.video.i_height = win_info.height;
    p_sys->fmt.video.i_bits_per_pixel = win_info.depth;
    p_sys->fmt.video.i_chroma = i_chroma;

#if 0
    win_info.visual->red_mask;
    win_info.visual->green_mask;
    win_info.visual->blue_mask;
    win_info.visual->bits_per_rgb;
#endif

    return VLC_SUCCESS;
}

int screen_CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    Display *p_display = (Display *)p_sys->p_data;

    XCloseDisplay( p_display );
    if( p_sys->p_blend )
    {
        module_unneed( p_sys->p_blend, p_sys->p_blend->p_module );
        vlc_object_detach( p_sys->p_blend );
        vlc_object_release( p_sys->p_blend );
    }
    return VLC_SUCCESS;
}

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    Display *p_display = (Display *)p_sys->p_data;
    block_t *p_block;
    XImage *image;
    int i_size;
    int root_x = 0, root_y = 0;

    if( p_sys->b_follow_mouse || p_sys->p_mouse )
    {
        Window root = DefaultRootWindow( p_display ), child;
        int win_x, win_y;
        unsigned int mask;
        if( XQueryPointer( p_display, root,
            &root, &child, &root_x, &root_y, &win_x, &win_y,
            &mask ) )
        {
            if( p_sys->b_follow_mouse )
                FollowMouse( p_sys, root_x, root_y );
        }
        else
            msg_Dbg( p_demux, "XQueryPointer() failed" );

    }

    image = XGetImage( p_display, DefaultRootWindow( p_display ),
                       p_sys->i_left, p_sys->i_top, p_sys->fmt.video.i_width,
                       p_sys->fmt.video.i_height, AllPlanes, ZPixmap );

    if( !image )
    {
        msg_Warn( p_demux, "cannot get image" );
        return NULL;
    }

    i_size = image->bytes_per_line * image->height;

    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "cannot get block" );
        XDestroyImage( image );
        return NULL;
    }

    vlc_memcpy( p_block->p_buffer, image->data, i_size );

    if( p_sys->p_mouse )
        RenderCursor( p_demux, root_x, root_y,
                      p_block->p_buffer );

    XDestroyImage( image );

    return p_block;
}

