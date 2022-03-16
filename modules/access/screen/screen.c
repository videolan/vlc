/*****************************************************************************
 * screen.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004-2008 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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
#include <vlc_plugin.h>
#include <vlc_modules.h>                 /* module_need for "video blending" */
#include <vlc_filter.h>
#include <vlc_url.h>
#include "screen.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "Desired frame rate for the capture." )

#ifdef _WIN32
#define FRAGS_TEXT N_("Capture fragment size")
#define FRAGS_LONGTEXT N_( \
    "Optimize the capture by fragmenting the screen in chunks " \
    "of predefined height (16 might be a good value, and 0 means disabled)." )
#endif

#ifdef SCREEN_SUBSCREEN
#define TOP_TEXT N_( "Region top row" )
#define TOP_LONGTEXT N_( \
    "Ordinate of the capture region in pixels." )

#define LEFT_TEXT N_( "Region left column" )
#define LEFT_LONGTEXT N_( \
    "Abscissa of the capture region in pixels." )

#define WIDTH_TEXT N_( "Capture region width" )

#define HEIGHT_TEXT N_( "Capture region height" )

#define FOLLOW_MOUSE_TEXT N_( "Follow the mouse" )
#define FOLLOW_MOUSE_LONGTEXT N_( \
    "Follow the mouse when capturing a subscreen." )
#endif

#ifdef SCREEN_MOUSE
#define MOUSE_TEXT N_( "Mouse pointer image" )
#define MOUSE_LONGTEXT N_( \
    "If specified, will use the image to draw the mouse pointer on the " \
    "capture." )
#endif

#ifdef SCREEN_DISPLAY_ID
#define DISPLAY_ID_TEXT N_( "Display ID" )
#define DISPLAY_ID_LONGTEXT N_( \
    "Display ID. If not specified, main display ID is used." )
#define INDEX_TEXT N_( "Screen index" )
#define INDEX_LONGTEXT N_( \
    "Index of screen (1, 2, 3, ...). Alternative to Display ID." )
#endif

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#ifdef _WIN32
#   define SCREEN_FPS 1
#else
#   define SCREEN_FPS 5
#endif

vlc_module_begin ()
    set_description( N_("Screen Input") )
    set_shortname( N_("Screen" ))
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_float( "screen-fps", SCREEN_FPS, FPS_TEXT, FPS_LONGTEXT )

#ifdef SCREEN_SUBSCREEN
    add_integer( "screen-top", 0, TOP_TEXT, TOP_LONGTEXT )
    add_integer( "screen-left", 0, LEFT_TEXT, LEFT_LONGTEXT )
    add_integer( "screen-width", 0, WIDTH_TEXT, NULL )
    add_integer( "screen-height", 0, HEIGHT_TEXT, NULL )

    add_bool( "screen-follow-mouse", false, FOLLOW_MOUSE_TEXT,
              FOLLOW_MOUSE_LONGTEXT )
#endif

#ifdef SCREEN_MOUSE
    add_loadfile("screen-mouse-image", "", MOUSE_TEXT, MOUSE_LONGTEXT)
#endif

#ifdef _WIN32
    add_integer( "screen-fragment-size", 0, FRAGS_TEXT, FRAGS_LONGTEXT )
#endif

#ifdef SCREEN_DISPLAY_ID
    add_integer( "screen-display-id", 0, DISPLAY_ID_TEXT, DISPLAY_ID_LONGTEXT )
    add_integer( "screen-index", 0, INDEX_TEXT, INDEX_LONGTEXT )
#endif

    set_capability( "access", 0 )
    add_shortcut( "screen" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control( demux_t *, int, va_list );
static int Demux  ( demux_t * );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->f_fps = var_CreateGetFloat( p_demux, "screen-fps" );
    p_sys->i_incr = vlc_tick_rate_duration( p_sys->f_fps );
    p_sys->i_next_date = 0;

#ifdef SCREEN_SUBSCREEN
    p_sys->i_top = var_CreateGetInteger( p_demux, "screen-top" );
    p_sys->i_left = var_CreateGetInteger( p_demux, "screen-left" );
    p_sys->i_width = var_CreateGetInteger( p_demux, "screen-width" );
    p_sys->i_height = var_CreateGetInteger( p_demux, "screen-height" );
#endif

#ifdef SCREEN_DISPLAY_ID
    p_sys->i_display_id = var_CreateGetInteger( p_demux, "screen-display-id" );
    p_sys->i_screen_index = var_CreateGetInteger( p_demux, "screen-index" );
#endif

    if( screen_InitCapture( p_demux ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "screen width: %i, height: %i, depth: %i",
             p_sys->fmt.video.i_width, p_sys->fmt.video.i_height,
             p_sys->fmt.video.i_bits_per_pixel );

#ifdef SCREEN_SUBSCREEN
    if( p_sys->i_left >= p_sys->fmt.video.i_width
     || p_sys->i_top >= p_sys->fmt.video.i_height )
    {
        msg_Err( p_demux, "subscreen left/top out of range" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->i_width == 0 )
        p_sys->i_width = p_sys->fmt.video.i_width - p_sys->i_left;
    if( p_sys->i_height == 0 )
        p_sys->i_height = p_sys->fmt.video.i_height - p_sys->i_top;

    if( p_sys->i_left + p_sys->i_width > p_sys->fmt.video.i_width ||
        p_sys->i_top + p_sys->i_height > p_sys->fmt.video.i_height )
    {
        msg_Err( p_demux, "subscreen region overflows the screen" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    else
    {
        p_sys->i_screen_width = p_sys->fmt.video.i_width;
        p_sys->i_screen_height = p_sys->fmt.video.i_height;
        p_sys->fmt.video.i_visible_width =
        p_sys->fmt.video.i_width = p_sys->i_width;
        p_sys->fmt.video.i_visible_height =
        p_sys->fmt.video.i_height = p_sys->i_height;
        p_sys->b_follow_mouse = var_CreateGetBool( p_demux, "screen-follow-mouse" );
        if( p_sys->b_follow_mouse )
            msg_Dbg( p_demux, "mouse following enabled" );
    }

    msg_Dbg( p_demux, "capturing subscreen top: %d, left: %d, width: %d, height: %d",
             p_sys->i_top, p_sys->i_left, p_sys->i_width, p_sys->i_height );
#endif

#ifdef SCREEN_MOUSE
    char *mousefile = var_InheritString( p_demux, "screen-mouse-image" );
    char *mouseurl = mousefile ? vlc_path2uri( mousefile, NULL ) : NULL;
    free( mousefile );
    if( mouseurl )
    {
        image_handler_t *p_image;
        video_format_t fmt_out;
        msg_Dbg( p_demux, "Using %s for the mouse pointer image", mouseurl );
        video_format_Init( &fmt_out, VLC_CODEC_RGBA );
        p_image = image_HandlerCreate( p_demux );
        if( p_image )
        {
            p_sys->p_mouse =
                image_ReadUrl( p_image, mouseurl, &fmt_out );
            image_HandlerDelete( p_image );
        }
        if( !p_sys->p_mouse )
            msg_Err( p_demux, "Failed to open mouse pointer image (%s)",
                     mouseurl );
        free( mouseurl );
        video_format_Clean( &fmt_out );
    }
#endif

    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );

    p_sys->i_start = vlc_tick_now();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    screen_CloseCapture( p_demux );
#ifdef SCREEN_MOUSE
    if( p_sys->p_mouse )
        picture_Release( p_sys->p_mouse );
    if( p_sys->p_blend )
    {
        filter_Close( p_sys->p_blend );
        module_unneed( p_sys->p_blend, p_sys->p_blend->p_module );
        vlc_object_delete(p_sys->p_blend);
    }
#endif
    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;

    if( !p_sys->i_next_date ) p_sys->i_next_date = vlc_tick_now();

    /* Frame skipping if necessary */
    while( vlc_tick_now() >= p_sys->i_next_date + p_sys->i_incr )
        p_sys->i_next_date += p_sys->i_incr;

    vlc_tick_wait( p_sys->i_next_date );
    p_block = screen_Capture( p_demux );
    if( !p_block )
    {
        p_sys->i_next_date += p_sys->i_incr;
        return 1;
    }

    p_block->i_dts = p_block->i_pts = p_sys->i_next_date;

    es_out_SetPCR( p_demux->out, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->es, p_block );

    p_sys->i_next_date += p_sys->i_incr;

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool *pb;
    demux_sys_t *p_sys = p_demux->p_sys;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_demux, "live-caching" ));
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = vlc_tick_now() - p_sys->i_start;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}

#ifdef SCREEN_SUBSCREEN
void FollowMouse( demux_sys_t *p_sys, int i_x, int i_y )
{
    i_x -= p_sys->i_width/2;
    if( i_x < 0 ) i_x = 0;
    p_sys->i_left = __MIN( (unsigned int)i_x,
    p_sys->i_screen_width - p_sys->i_width );

    i_y -= p_sys->i_height/2;
    if( i_y < 0 ) i_y = 0;
    p_sys->i_top = __MIN( (unsigned int)i_y,
    p_sys->i_screen_height - p_sys->i_height );
}
#endif

#ifdef SCREEN_MOUSE
void RenderCursor( demux_t *p_demux, int i_x, int i_y,
                   uint8_t *p_dst )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( !p_sys->dst.i_planes )
        picture_Setup( &p_sys->dst, &p_sys->fmt.video );

    if( !p_sys->dst.i_planes )
        return;

#ifdef _WIN32
    /* Bitmaps here created by CreateDIBSection: stride rounded up to the nearest DWORD */
    p_sys->dst.p[ 0 ].i_pitch = p_sys->dst.p[ 0 ].i_visible_pitch =
        ( ( ( ( p_sys->fmt.video.i_width * p_sys->fmt.video.i_bits_per_pixel ) + 31 ) & ~31 ) >> 3 );
#endif

    if( !p_sys->p_blend )
    {
        p_sys->p_blend = vlc_object_create( p_demux, sizeof(filter_t) );
        if( p_sys->p_blend )
        {
            es_format_Init( &p_sys->p_blend->fmt_in, VIDEO_ES,
                            VLC_CODEC_RGBA );
            p_sys->p_blend->fmt_in.video = p_sys->p_mouse->format;
            p_sys->p_blend->fmt_out = p_sys->fmt;
            p_sys->p_blend->p_module =
                module_need( p_sys->p_blend, "video blending", NULL, false );
            if( !p_sys->p_blend->p_module )
            {
                msg_Err( p_demux, "Could not load video blending module" );
                vlc_object_delete(p_sys->p_blend);
                p_sys->p_blend = NULL;
            }
            assert( p_sys->p_blend->ops != NULL );
        }
    }
    if( p_sys->p_blend )
    {
        p_sys->dst.p->p_pixels = p_dst;
        p_sys->p_blend->ops->blend_video( p_sys->p_blend,
                                        &p_sys->dst,
                                        p_sys->p_mouse,
#ifdef SCREEN_SUBSCREEN
                                        i_x-p_sys->i_left,
#else
                                        i_x,
#endif
#ifdef SCREEN_SUBSCREEN
                                        i_y-p_sys->i_top,
#else
                                        i_y,
#endif
                                        255 );
    }
    else
    {
        picture_Release( p_sys->p_mouse );
        p_sys->p_mouse = NULL;
    }
}
#endif
