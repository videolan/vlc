/*****************************************************************************
 * caca.c: Color ASCII Art video output plugin using libcaca
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
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
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_keys.h>

#include <caca.h>

#ifndef CACA_API_VERSION_1
    /* Upward compatibility macros */
    typedef char cucul_canvas_t;
    typedef struct caca_bitmap cucul_dither_t;
    typedef char caca_display_t;
#   define CUCUL_COLOR_DEFAULT CACA_COLOR_LIGHTGRAY
#   define CUCUL_COLOR_BLACK CACA_COLOR_BLACK
#   define cucul_clear_canvas(x) caca_clear()
#   define cucul_create_canvas(x,y) "" /* kinda hacky */
#   define cucul_create_dither caca_create_bitmap
#   define cucul_dither_bitmap(x,y,z,t,u,v,w) caca_draw_bitmap(y,z,t,u,v,w)
#   define cucul_free_dither caca_free_bitmap
#   define cucul_free_canvas(x)
#   define cucul_get_canvas_width(x) caca_get_width()
#   define cucul_get_canvas_height(x) caca_get_height()
#   define cucul_set_color(x,y,z) caca_set_color(y,z)
#   define caca_create_display(x) (caca_init() ? NULL : "") /* hacky, too */
#   define caca_free_display(x) caca_end()
#   define caca_get_event(x,y,z,t) *(z) = caca_get_event(y)
#   define caca_refresh_display(x) caca_refresh()
#   define caca_set_display_title(x,y) caca_set_window_title(y)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
static void Display   ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "Caca" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_description( N_("Color ASCII art video output") )
    set_capability( "video output", 12 )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * vout_sys_t: libcaca video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the libcaca specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    cucul_canvas_t *p_cv;
    caca_display_t *p_dp;
    cucul_dither_t *p_dither;
};

/*****************************************************************************
 * Create: allocates libcaca video output thread
 *****************************************************************************
 * This function initializes libcaca vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

#if defined( WIN32 ) && !defined( UNDER_CE )
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    SMALL_RECT rect;
    COORD coord;
    HANDLE hstdout;

    if( !AllocConsole() )
    {
        msg_Err( p_vout, "cannot create console" );
        return VLC_EGENERIC;
    }

    hstdout =
        CreateConsoleScreenBuffer( GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, CONSOLE_TEXTMODE_BUFFER, NULL );
    if( !hstdout || hstdout == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_vout, "cannot create screen buffer" );
        FreeConsole();
        return VLC_EGENERIC;
    }

    if( !SetConsoleActiveScreenBuffer( hstdout) )
    {
        msg_Err( p_vout, "cannot set active screen buffer" );
        FreeConsole();
        return VLC_EGENERIC;
    }

    coord = GetLargestConsoleWindowSize( hstdout );
    msg_Dbg( p_vout, "SetConsoleWindowInfo: %ix%i", coord.X, coord.Y );

    /* Force size for now */
    coord.X = 100;
    coord.Y = 40;

    if( !SetConsoleScreenBufferSize( hstdout, coord ) )
        msg_Warn( p_vout, "SetConsoleScreenBufferSize %i %i",
                  coord.X, coord.Y );

    /* Get the current screen buffer size and window position. */
    if( GetConsoleScreenBufferInfo( hstdout, &csbiInfo ) )
    {
        rect.Top = 0; rect.Left = 0;
        rect.Right = csbiInfo.dwMaximumWindowSize.X - 1;
        rect.Bottom = csbiInfo.dwMaximumWindowSize.Y - 1;
        if( !SetConsoleWindowInfo( hstdout, TRUE, &rect ) )
            msg_Dbg( p_vout, "SetConsoleWindowInfo failed: %ix%i",
                     rect.Right, rect.Bottom );
    }
#endif

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
#if defined( WIN32 ) && !defined( UNDER_CE )
        FreeConsole();
#endif
        return VLC_ENOMEM;
    }

    p_vout->p_sys->p_cv = cucul_create_canvas(0, 0);
    if( !p_vout->p_sys->p_cv )
    {
        msg_Err( p_vout, "cannot initialize libcucul" );
#if defined( WIN32 ) && !defined( UNDER_CE )
        FreeConsole();
#endif
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->p_dp = caca_create_display( p_vout->p_sys->p_cv );
    if( !p_vout->p_sys->p_dp )
    {
        msg_Err( p_vout, "cannot initialize libcaca" );
        cucul_free_canvas( p_vout->p_sys->p_cv );
#if defined( WIN32 ) && !defined( UNDER_CE )
        FreeConsole();
#endif
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    caca_set_display_title( p_vout->p_sys->p_dp,
                            VOUT_TITLE " - Colour AsCii Art (caca)" );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize libcaca video output thread
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic = NULL;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
    p_vout->output.i_width = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->output.i_rmask = 0x00ff0000;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x000000ff;

    /* Create the libcaca dither object */
    p_vout->p_sys->p_dither = cucul_create_dither
                       ( 32, p_vout->output.i_width, p_vout->output.i_height,
                         4 * ((p_vout->output.i_width + 15) & ~15),
                         p_vout->output.i_rmask, p_vout->output.i_gmask,
                         p_vout->output.i_bmask, 0x00000000 );

    if( !p_vout->p_sys->p_dither )
    {
        msg_Err( p_vout, "could not create libcaca dither object" );
        return VLC_EGENERIC;
    }

    /* Find an empty picture slot */
    for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
    {
        if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
        {
            p_pic = p_vout->p_picture + i_index;
            break;
        }
    }

    if( p_pic == NULL )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the picture */
    p_pic->p->i_lines = p_vout->output.i_height;
    p_pic->p->i_visible_lines = p_vout->output.i_height;
    p_pic->p->i_pitch = 4 * ((p_vout->output.i_width + 15) & ~15);
    p_pic->p->i_pixel_pitch = 4;
    p_pic->p->i_visible_pitch = 4 * p_vout->output.i_width;
    p_pic->i_planes = 1;
    p_pic->p->p_pixels = malloc( p_pic->p->i_pitch * p_pic->p->i_lines );

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;
    I_OUTPUTPICTURES++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate libcaca video output thread
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    cucul_free_dither( p_vout->p_sys->p_dither );
}

/*****************************************************************************
 * Destroy: destroy libcaca video output thread
 *****************************************************************************
 * Terminate an output method created by AaCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    caca_free_display( p_vout->p_sys->p_dp );
    cucul_free_canvas( p_vout->p_sys->p_cv );

#if defined( WIN32 ) && !defined( UNDER_CE )
    FreeConsole();
#endif

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Manage: handle libcaca events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
#ifdef CACA_API_VERSION_1
    struct caca_event ev;
#else
    int ev;
#endif

    while( caca_get_event(p_vout->p_sys->p_dp, CACA_EVENT_ANY, &ev, 0) )
    {
        playlist_t *p_playlist;
        vlc_value_t val;

#ifdef CACA_API_VERSION_1
#ifdef CACA_EVENT_OPAQUE
        switch( caca_get_event_type( &ev ) )
#else
        switch( ev.type )
#endif /* CACA_EVENT_OPAQUE */
#else
        switch( ev )
#endif
        {
        case CACA_EVENT_KEY_RELEASE:
#ifdef CACA_API_VERSION_1
#ifdef CACA_EVENT_OPAQUE
            switch( caca_get_event_key_ch( &ev ) )
#else
            switch( ev.data.key.ch )
#endif /* CACA_EVENT_OPAQUE */
#else
            switch( ev & 0x00ffffff )
#endif
            {
            case 'q':
                val.i_int = KEY_MODIFIER_CTRL | 'q';
                break;
            case ' ':
                val.i_int = KEY_SPACE;
                break;
            default:
                continue;
            }

            var_Set( p_vout->p_libvlc, "key-pressed", val );
            break;
        case CACA_EVENT_RESIZE:
            /* Acknowledge the resize */
            caca_refresh_display( p_vout->p_sys->p_dp );
            break;
#ifdef CACA_API_VERSION_1
        case  CACA_EVENT_MOUSE_MOTION:
            val.i_int =
#ifdef CACA_EVENT_OPAQUE
                caca_get_event_mouse_x( &ev )
#else
                ev.data.mouse.x
#endif /* CACA_EVENT_OPAQUE */
                * p_vout->render.i_width
                         / cucul_get_canvas_width( p_vout->p_sys->p_cv );
            var_Set( p_vout, "mouse-x", val );
            val.i_int =
#ifdef CACA_EVENT_OPAQUE
                caca_get_event_mouse_y( &ev ) 
#else
                ev.data.mouse.y
#endif /* CACA_EVENT_OPAQUE */
                * p_vout->render.i_height
                         / cucul_get_canvas_height( p_vout->p_sys->p_cv );
            var_Set( p_vout, "mouse-y", val );
            var_SetBool( p_vout, "mouse-moved", true );
            break;
        case CACA_EVENT_MOUSE_RELEASE:
            var_SetBool( p_vout, "mouse-clicked", true );
            break;
        case CACA_EVENT_QUIT:
        {
            p_playlist = pl_Hold( p_vout );
            if( p_playlist )
            {
                playlist_Stop( p_playlist );
                pl_Release( p_vout );
            }
            libvlc_Quit( p_vout->p_libvlc );
            break;
        }
#endif
        default:
            break;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    cucul_set_color_ansi( p_vout->p_sys->p_cv,
                     CUCUL_COLOR_DEFAULT, CUCUL_COLOR_BLACK );
    cucul_clear_canvas( p_vout->p_sys->p_cv );
    cucul_dither_bitmap( p_vout->p_sys->p_cv, 0, 0,
                         cucul_get_canvas_width( p_vout->p_sys->p_cv ) - 1,
                         cucul_get_canvas_height( p_vout->p_sys->p_cv ) - 1,
                         p_vout->p_sys->p_dither, p_pic->p->p_pixels );
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    VLC_UNUSED(p_pic);
    caca_refresh_display( p_vout->p_sys->p_dp );
}

