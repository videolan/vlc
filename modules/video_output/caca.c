/*****************************************************************************
 * caca.c: Color ASCII Art video output plugin using libcaca
 *****************************************************************************
 * Copyright (C) 2003, 2004 the VideoLAN team
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <caca.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/intf.h>
#include <vlc_keys.h>

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
vlc_module_begin();
    set_shortname( "Caca" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    set_description( _("Color ASCII art video output") );
    set_capability( "video output", 12 );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: libcaca video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the libcaca specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
#ifdef CACA_API_VERSION_1
    struct cucul_context *p_qq;
    struct caca_context *p_kk;
    struct cucul_bitmap *p_bitmap;
#else
    struct caca_bitmap *p_bitmap;
#endif
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
    if( AllocConsole() )
    {
        CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
        SMALL_RECT rect;
        COORD coord;

        HANDLE hstdout =
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
    }
    else
    {
        msg_Err( p_vout, "cannot create console" );
        return VLC_EGENERIC;
    }

#endif

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

#ifdef CACA_API_VERSION_1
    p_vout->p_sys->p_qq = cucul_init(0, 0);
    if( !p_vout->p_sys->p_qq )
    {
        msg_Err( p_vout, "cannot initialize libcucul" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->p_kk = caca_attach( p_vout->p_sys->p_qq );
    if( !p_vout->p_sys->p_kk )
    {
        msg_Err( p_vout, "cannot initialize libcaca" );
        cucul_end( p_vout->p_sys->p_qq );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    caca_set_window_title( p_vout->p_sys->p_kk,
                           VOUT_TITLE " - Colour AsCii Art (caca)" );
#else
    if( caca_init() )
    {
        msg_Err( p_vout, "cannot initialize libcaca" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    caca_set_window_title( VOUT_TITLE " - Colour AsCii Art (caca)" );
#endif

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

    /* Create the libcaca bitmap */
    p_vout->p_sys->p_bitmap =
#ifdef CACA_API_VERSION_1
        cucul_create_bitmap
#else
        caca_create_bitmap
#endif
                       ( 32, p_vout->output.i_width, p_vout->output.i_height,
                         4 * ((p_vout->output.i_width + 15) & ~15),
                         p_vout->output.i_rmask, p_vout->output.i_gmask,
                         p_vout->output.i_bmask, 0x00000000 );

    if( !p_vout->p_sys->p_bitmap )
    {
        msg_Err( p_vout, "could not create libcaca bitmap" );
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
#ifdef CACA_API_VERSION_1
    cucul_free_bitmap( p_vout->p_sys->p_bitmap );
#else
    caca_free_bitmap( p_vout->p_sys->p_bitmap );
#endif
}

/*****************************************************************************
 * Destroy: destroy libcaca video output thread
 *****************************************************************************
 * Terminate an output method created by AaCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

#ifdef CACA_API_VERSION_1
    caca_detach( p_vout->p_sys->p_kk );
    cucul_end( p_vout->p_sys->p_qq );
#else
    caca_end();
#endif

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
    int event;
    vlc_value_t val;

#ifdef CACA_API_VERSION_1
    while(( event = caca_get_event(p_vout->p_sys->p_kk,
                                   CACA_EVENT_KEY_PRESS | CACA_EVENT_RESIZE) ))
#else
    while(( event = caca_get_event(CACA_EVENT_KEY_PRESS | CACA_EVENT_RESIZE) ))
#endif
    {
        if( event == CACA_EVENT_RESIZE )
        {
            /* Acknowledge the resize */
#ifdef CACA_API_VERSION_1
            caca_display( p_vout->p_sys->p_kk );
#else
            caca_refresh();
#endif
            continue;
        }

        switch( event & 0x00ffffff )
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

        var_Set( p_vout->p_vlc, "key-pressed", val );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef CACA_API_VERSION_1
    cucul_clear( p_vout->p_sys->p_qq );
    cucul_draw_bitmap( p_vout->p_sys->p_qq,
                       0, 0,
                       cucul_get_width( p_vout->p_sys->p_qq ) - 1,
                       cucul_get_height( p_vout->p_sys->p_qq ) - 1,
                       p_vout->p_sys->p_bitmap, p_pic->p->p_pixels );
#else
    caca_clear();
    caca_draw_bitmap( 0, 0, caca_get_width() - 1, caca_get_height() - 1,
                      p_vout->p_sys->p_bitmap, p_pic->p->p_pixels );
#endif
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef CACA_API_VERSION_1
    caca_display( p_vout->p_sys->p_kk );
#else
    caca_refresh();
#endif
}

