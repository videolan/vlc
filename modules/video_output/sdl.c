/*****************************************************************************
 * sdl.c: SDL video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Pierre Baillet <oct@zoy.org>
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_keys.h>
//#include <vlc_aout.h>

#include <sys/types.h>
#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#include SDL_INCLUDE_FILE

/* SDL is not able to crop overlays - so use only 1 direct buffer */
#define SDL_MAX_DIRECTBUFFERS 1
#define SDL_DEFAULT_BPP 16

/*****************************************************************************
 * vout_sys_t: video output SDL method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the SDL specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    SDL_Surface *   p_display;                             /* display device */

    int i_width;
    int i_height;

#if SDL_VERSION_ATLEAST(1,2,10)
    unsigned int i_desktop_width;
    unsigned int i_desktop_height;
#endif

    /* For YUV output */
    SDL_Overlay * p_overlay;   /* An overlay we keep to grab the XVideo port */

    /* For RGB output */
    int i_surfaces;

    bool  b_cursor;
    bool  b_cursor_autohidden;
    mtime_t     i_lastmoved;
    mtime_t     i_mouse_hide_timeout;
    mtime_t     i_lastpressed;                        /* to track dbl-clicks */
};

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * SDL specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_t
{
    SDL_Overlay *p_overlay;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open      ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );

static int  OpenDisplay     ( vout_thread_t * );
static void CloseDisplay    ( vout_thread_t * );
static int  NewPicture      ( vout_thread_t *, picture_t * );
static void SetPalette      ( vout_thread_t *,
                              uint16_t *, uint16_t *, uint16_t * );

static int ConvertKey( SDLKey );


#define CHROMA_TEXT N_("SDL chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the SDL renderer to use a specific chroma format instead of " \
    "trying to improve performances by using the most efficient one.")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "SDL" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_description( N_("Simple DirectMedia Layer video output") )
    set_capability( "video output", 60 )
    add_shortcut( "sdl" )
    add_string( "sdl-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )
    set_callbacks( Open, Close )
#if defined( __i386__ ) || defined( __x86_64__ )
    /* On i386, SDL is linked against svgalib */
    linked_with_a_crap_library_which_uses_atexit ()
#endif
vlc_module_end ()

static vlc_mutex_t sdl_lock = VLC_STATIC_MUTEX;

/*****************************************************************************
 * OpenVideo: allocate SDL video thread output method
 *****************************************************************************
 * This function allocate and initialize a SDL vout method. It uses some of the
 * vout properties to choose the correct mode, and change them according to the
 * mode actually used.
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    /* XXX: check for conflicts with the SDL audio output */
    vlc_mutex_lock( &sdl_lock );

#ifdef HAVE_SETENV
    char *psz_method;
#endif

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        vlc_mutex_unlock( &sdl_lock );
        return VLC_ENOMEM;
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    /* Check if SDL video module has been initialized */
    if( SDL_WasInit( SDL_INIT_VIDEO ) != 0 )
    {
        vlc_mutex_unlock( &sdl_lock );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;
    p_vout->pf_control = NULL;

#ifdef HAVE_SETENV
    char* psz = psz_method = config_GetPsz( p_vout, "vout" );
    if( psz_method )
    {
        while( *psz_method && *psz_method != ':' )
        {
            psz_method++;
        }

        if( *psz_method )
        {
            setenv( "SDL_VIDEODRIVER", psz_method + 1, 1 );
        }
    }
    free( psz );
#endif

    /* Initialize library */
    if( SDL_Init( SDL_INIT_VIDEO
#ifndef WIN32
    /* Win32 SDL implementation doesn't support SDL_INIT_EVENTTHREAD yet*/
                | SDL_INIT_EVENTTHREAD
#endif
#ifndef NDEBUG
    /* In debug mode you may want vlc to dump a core instead of staying
     * stuck */
                | SDL_INIT_NOPARACHUTE
#endif
                ) < 0 )
    {
        msg_Err( p_vout, "cannot initialize SDL (%s)", SDL_GetError() );
        free( p_vout->p_sys );
        vlc_mutex_unlock( &sdl_lock );
        return VLC_EGENERIC;
    }

    vlc_mutex_unlock( &sdl_lock );

    /* Translate keys into unicode */
    SDL_EnableUNICODE(1);

    /* Get the desktop resolution */
#if SDL_VERSION_ATLEAST(1,2,10)
    /* FIXME: SDL has a problem with virtual desktop */
    p_vout->p_sys->i_desktop_width = SDL_GetVideoInfo()->current_w;
    p_vout->p_sys->i_desktop_height = SDL_GetVideoInfo()->current_h;
#endif

    /* Create the cursor */
    p_vout->p_sys->b_cursor = 1;
    p_vout->p_sys->b_cursor_autohidden = 0;
    p_vout->p_sys->i_lastmoved = p_vout->p_sys->i_lastpressed = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;

    if( OpenDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot set up SDL (%s)", SDL_GetError() );
        SDL_QuitSubSystem( SDL_INIT_VIDEO );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize SDL video thread output method
 *****************************************************************************
 * This function initialize the SDL display device.
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    p_vout->p_sys->i_surfaces = 0;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    if( p_vout->p_sys->p_overlay == NULL )
    {
        /* All we have is an RGB image with square pixels */
        p_vout->output.i_width  = p_vout->p_sys->i_width;
        p_vout->output.i_height = p_vout->p_sys->i_height;
        p_vout->output.i_aspect = p_vout->output.i_width
                                   * VOUT_ASPECT_FACTOR
                                   / p_vout->output.i_height;
    }
    else
    {
        /* We may need to convert the chroma, but at least we keep the
         * aspect ratio */
        p_vout->output.i_width  = p_vout->render.i_width;
        p_vout->output.i_height = p_vout->render.i_height;
        p_vout->output.i_aspect = p_vout->render.i_aspect;
    }

    /* Try to initialize SDL_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < SDL_MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture if we found one */
        if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by OpenVideo
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        if( p_vout->p_sys->p_overlay == NULL )
        {
            /* RGB picture */
        }
        else
        {
            SDL_UnlockYUVOverlay(
                    PP_OUTPUTPICTURE[ i_index ]->p_sys->p_overlay );
            SDL_FreeYUVOverlay(
                    PP_OUTPUTPICTURE[ i_index ]->p_sys->p_overlay );
        }
        free( PP_OUTPUTPICTURE[ i_index ]->p_sys );
    }
}

/*****************************************************************************
 * CloseVideo: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    CloseDisplay( p_vout );
    SDL_QuitSubSystem( SDL_INIT_VIDEO );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occurred.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    SDL_Event event;                                            /* SDL event */
    vlc_value_t val;
    unsigned int i_width, i_height, i_x, i_y;

    /* Process events */
    while( SDL_PollEvent( &event ) )
    {
        switch( event.type )
        {
        /* Resizing of window */
        case SDL_VIDEORESIZE:
            p_vout->i_changes |= VOUT_SIZE_CHANGE;
            p_vout->i_window_width = p_vout->p_sys->i_width = event.resize.w;
            p_vout->i_window_height = p_vout->p_sys->i_height = event.resize.h;
            break;

        /* Mouse move */
        case SDL_MOUSEMOTION:
            vout_PlacePicture( p_vout, p_vout->p_sys->i_width,
                               p_vout->p_sys->i_height,
                               &i_x, &i_y, &i_width, &i_height );

            /* Compute the x coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_width] */
            val.i_int = ( event.motion.x - i_x ) *
                        p_vout->fmt_in.i_visible_width / i_width +
                        p_vout->fmt_in.i_x_offset;

            if( (int)(event.motion.x - i_x) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_width )
                val.i_int = p_vout->fmt_in.i_visible_width;

            var_Set( p_vout, "mouse-x", val );

            /* compute the y coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_height] */
            val.i_int = ( event.motion.y - i_y ) *
                        p_vout->fmt_in.i_visible_height / i_height +
                        p_vout->fmt_in.i_y_offset;

            if( (int)(event.motion.y - i_y) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_height )
                val.i_int = p_vout->fmt_in.i_visible_height;

            var_Set( p_vout, "mouse-y", val );
            var_SetBool( p_vout, "mouse-moved", true );

            if( p_vout->p_sys->b_cursor )
            {
                if( p_vout->p_sys->b_cursor_autohidden )
                {
                    p_vout->p_sys->b_cursor_autohidden = 0;
                    SDL_ShowCursor( 1 );
                }
                else
                {
                    p_vout->p_sys->i_lastmoved = mdate();
                }
            }
            break;

        /* Mouse button released */
        case SDL_MOUSEBUTTONUP:
            switch( event.button.button )
            {
            case SDL_BUTTON_LEFT:
                {
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~1;
                    var_Set( p_vout, "mouse-button-down", val );

                    var_SetBool( p_vout, "mouse-clicked", true );
                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", false );
                }
                break;

            case SDL_BUTTON_MIDDLE:
                {
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~2;
                    var_Set( p_vout, "mouse-button-down", val );

                    var_Get( p_vout->p_libvlc, "intf-show", &val );
                    val.b_bool = !val.b_bool;
                    var_Set( p_vout->p_libvlc, "intf-show", val );
                }
                break;

            case SDL_BUTTON_RIGHT:
                {
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~4;
                    var_Set( p_vout, "mouse-button-down", val );

                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", true );
                }
                break;
            }
            break;

        /* Mouse button pressed */
        case SDL_MOUSEBUTTONDOWN:
            switch( event.button.button )
            {
            case SDL_BUTTON_LEFT:
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 1;
                var_Set( p_vout, "mouse-button-down", val );

                /* detect double-clicks */
                if( ( mdate() - p_vout->p_sys->i_lastpressed ) < 300000 )
                    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

                p_vout->p_sys->i_lastpressed = mdate();
                break;

            case SDL_BUTTON_MIDDLE:
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 2;
                var_Set( p_vout, "mouse-button-down", val );
                break;

            case SDL_BUTTON_RIGHT:
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 4;
                var_Set( p_vout, "mouse-button-down", val );
                break;
            }
            break;

        /* Quit event (close the window) */
        case SDL_QUIT:
            {
#if 0
                playlist_t *p_playlist = pl_Hold( p_vout );
                if( p_playlist != NULL )
                {
                    playlist_Stop( p_playlist );
                    pl_Release( p_vout );
                }
#else
#warning FIXME FIXME ?
#endif
            }
            break;

        /* Key pressed */
        case SDL_KEYDOWN:
            /* convert the key if possible */
            val.i_int = ConvertKey( event.key.keysym.sym );

            if( !val.i_int )
            {
                /* Find the right caracter */
                if( ( event.key.keysym.unicode & 0xff80 ) == 0 )
                {
                    val.i_int = event.key.keysym.unicode & 0x7f;
                    /* FIXME: find a better solution than this
                              hack to find the right caracter */
                    if( val.i_int >= 1 && val.i_int <= 26 )
                        val.i_int += 96;
                    else if( val.i_int >= 65 && val.i_int <= 90 )
                        val.i_int += 32;
                }
            }

            if( val.i_int )
            {
                if( ( event.key.keysym.mod & KMOD_SHIFT ) )
                {
                    val.i_int |= KEY_MODIFIER_SHIFT;
                }
                if( ( event.key.keysym.mod & KMOD_CTRL ) )
                {
                    val.i_int |= KEY_MODIFIER_CTRL;
                }
                if( ( event.key.keysym.mod & KMOD_ALT ) )
                {
                    val.i_int |= KEY_MODIFIER_ALT;
                }
                var_Set( p_vout->p_libvlc, "key-pressed", val );
            }

        default:
            break;
        }
    }

    /* Fullscreen change */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        vlc_value_t val_fs;

        /* Update the object variable and trigger callback */
        val_fs.b_bool = !p_vout->b_fullscreen;
        p_vout->b_fullscreen = !p_vout->b_fullscreen;
        var_Set( p_vout, "fullscreen", val_fs );

        /*TODO: add the "always on top" code here !*/

        p_vout->p_sys->b_cursor_autohidden = 0;
        SDL_ShowCursor( p_vout->p_sys->b_cursor &&
                        ! p_vout->p_sys->b_cursor_autohidden );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* autoscale toggle */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );
        p_vout->i_zoom = (int) ZOOM_FP_FACTOR;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* scaling factor (if no-autoscale) */
    if( p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_vout->b_autoscale = false;
        p_vout->i_zoom = (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* Crop or Aspect Ratio Changes */
    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* Size change */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        msg_Dbg( p_vout, "video display resized (%dx%d)",
                 p_vout->p_sys->i_width, p_vout->p_sys->i_height );

        CloseDisplay( p_vout );
        OpenDisplay( p_vout );

        /* We don't need to signal the vout thread about the size change if
         * we can handle rescaling ourselves */
        if( p_vout->p_sys->p_overlay != NULL )
            p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
    }

    /* Pointer change */
    if( ! p_vout->p_sys->b_cursor_autohidden &&
        ( mdate() - p_vout->p_sys->i_lastmoved >
            p_vout->p_sys->i_mouse_hide_timeout ) )
    {
        /* Hide the mouse automatically */
        p_vout->p_sys->b_cursor_autohidden = 1;
        SDL_ShowCursor( 0 );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Key events handling
 *****************************************************************************/
static const struct
{
    SDLKey sdl_key;
    int i_vlckey;
} sdlkeys_to_vlckeys[] =
{
    { SDLK_F1,  KEY_F1 },
    { SDLK_F2,  KEY_F2 },
    { SDLK_F3,  KEY_F3 },
    { SDLK_F4,  KEY_F4 },
    { SDLK_F5,  KEY_F5 },
    { SDLK_F6,  KEY_F6 },
    { SDLK_F7,  KEY_F7 },
    { SDLK_F8,  KEY_F8 },
    { SDLK_F9,  KEY_F9 },
    { SDLK_F10, KEY_F10 },
    { SDLK_F11, KEY_F11 },
    { SDLK_F12, KEY_F12 },

    { SDLK_RETURN, KEY_ENTER },
    { SDLK_KP_ENTER, KEY_ENTER },
    { SDLK_SPACE, KEY_SPACE },
    { SDLK_ESCAPE, KEY_ESC },

    { SDLK_MENU, KEY_MENU },
    { SDLK_LEFT, KEY_LEFT },
    { SDLK_RIGHT, KEY_RIGHT },
    { SDLK_UP, KEY_UP },
    { SDLK_DOWN, KEY_DOWN },

    { SDLK_HOME, KEY_HOME },
    { SDLK_END, KEY_END },
    { SDLK_PAGEUP, KEY_PAGEUP },
    { SDLK_PAGEDOWN,  KEY_PAGEDOWN },

    { SDLK_INSERT, KEY_INSERT },
    { SDLK_DELETE, KEY_DELETE },
    /*TODO: find a equivalent for SDL 
    { , KEY_MEDIA_NEXT_TRACK }
    { , KEY_MEDIA_PREV_TRACK }
    { , KEY_VOLUME_MUTE }
    { , KEY_VOLUME_DOWN }
    { , KEY_VOLUME_UP }
    { , KEY_MEDIA_PLAY_PAUSE }
    { , KEY_MEDIA_PLAY_PAUSE }*/

    { 0, 0 }
};

static int ConvertKey( SDLKey sdl_key )
{
    int i;
    for( i=0; sdlkeys_to_vlckeys[i].sdl_key != 0; i++ )
    {
        if( sdlkeys_to_vlckeys[i].sdl_key == sdl_key )
        {
            return sdlkeys_to_vlckeys[i].i_vlckey;
        }
    }
    return 0;
}


/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the display.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    unsigned int x, y, w, h;
    SDL_Rect disp;

    vout_PlacePicture( p_vout, p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                       &x, &y, &w, &h );
    disp.x = x;
    disp.y = y;
    disp.w = w;
    disp.h = h;

    if( p_vout->p_sys->p_overlay == NULL )
    {
        /* RGB picture */
        SDL_Flip( p_vout->p_sys->p_display );
    }
    else
    {
        /* Overlay picture */
        SDL_UnlockYUVOverlay( p_pic->p_sys->p_overlay);
        SDL_DisplayYUVOverlay( p_pic->p_sys->p_overlay , &disp );
        SDL_LockYUVOverlay( p_pic->p_sys->p_overlay);
    }
}

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: open and initialize SDL device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int OpenDisplay( vout_thread_t *p_vout )
{
    uint32_t i_flags;
    int i_bpp;

    /* SDL fucked up fourcc definitions on bigendian machines */
    uint32_t i_sdl_chroma;
    char *psz_chroma = NULL;
    uint32_t i_chroma = 0;

    bool b_overlay = config_GetInt( p_vout, "overlay" );

    /* Set main window's size */
#if SDL_VERSION_ATLEAST(1,2,10)
    p_vout->p_sys->i_width = p_vout->b_fullscreen ? p_vout->p_sys->i_desktop_width :
                                                    p_vout->i_window_width;
    p_vout->p_sys->i_height = p_vout->b_fullscreen ? p_vout->p_sys->i_desktop_height :
                                                     p_vout->i_window_height;
#else
    p_vout->p_sys->i_width = p_vout->b_fullscreen ? p_vout->output.i_width :
                                                    p_vout->i_window_width;
    p_vout->p_sys->i_height = p_vout->b_fullscreen ? p_vout->output.i_height :
                                                     p_vout->i_window_height;
#endif

    /* Initialize flags and cursor */
    i_flags = SDL_ANYFORMAT | SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF;
    i_flags |= p_vout->b_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;

    i_bpp = SDL_VideoModeOK( p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                             SDL_DEFAULT_BPP, i_flags );
    if( i_bpp == 0 )
    {
        msg_Err( p_vout, "no video mode available" );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->p_display = SDL_SetVideoMode( p_vout->p_sys->i_width,
                                                 p_vout->p_sys->i_height,
                                                 i_bpp, i_flags );

    if( p_vout->p_sys->p_display == NULL )
    {
        msg_Err( p_vout, "cannot set video mode" );
        return VLC_EGENERIC;
    }

    SDL_LockSurface( p_vout->p_sys->p_display );

    if( ( psz_chroma = config_GetPsz( p_vout, "sdl-chroma" ) ) )
    {
        if( strlen( psz_chroma ) >= 4 )
        {
            memcpy(&i_chroma, psz_chroma, 4);
            msg_Dbg( p_vout, "Forcing chroma to 0x%.8x (%4.4s)", i_chroma, (char*)&i_chroma );
        }
        else
        {
            free( psz_chroma );
            psz_chroma = NULL;
        }
    }

    if( b_overlay )
    {
        /* Choose the chroma we will try first. */
        do
        {
            if( !psz_chroma ) i_chroma = 0;
            switch( i_chroma ? i_chroma : p_vout->render.i_chroma )
            {
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('Y','U','N','V'):
                    p_vout->output.i_chroma = VLC_FOURCC('Y','U','Y','2');
                    i_sdl_chroma = SDL_YUY2_OVERLAY;
                    break;
                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('U','Y','N','V'):
                case VLC_FOURCC('Y','4','2','2'):
                    p_vout->output.i_chroma = VLC_FOURCC('U','Y','V','Y');
                    i_sdl_chroma = SDL_UYVY_OVERLAY;
                    break;
                case VLC_FOURCC('Y','V','Y','U'):
                    p_vout->output.i_chroma = VLC_FOURCC('Y','V','Y','U');
                    i_sdl_chroma = SDL_YVYU_OVERLAY;
                    break;
                case VLC_FOURCC('Y','V','1','2'):
                case VLC_FOURCC('I','4','2','0'):
                case VLC_FOURCC('I','Y','U','V'):
                default:
                    p_vout->output.i_chroma = VLC_FOURCC('Y','V','1','2');
                    i_sdl_chroma = SDL_YV12_OVERLAY;
                    break;
            }
            free( psz_chroma ); psz_chroma = NULL;

            p_vout->p_sys->p_overlay =
                SDL_CreateYUVOverlay( 32, 32, i_sdl_chroma,
                                      p_vout->p_sys->p_display );
            /* FIXME: if the first overlay we find is software, don't stop,
             * because we may find a hardware one later ... */
        }
        while( i_chroma && !p_vout->p_sys->p_overlay );


        /* If this best choice failed, fall back to other chromas */
        if( p_vout->p_sys->p_overlay == NULL )
        {
            p_vout->output.i_chroma = VLC_FOURCC('I','Y','U','V');
            p_vout->p_sys->p_overlay =
                SDL_CreateYUVOverlay( 32, 32, SDL_IYUV_OVERLAY,
                                      p_vout->p_sys->p_display );
        }

        if( p_vout->p_sys->p_overlay == NULL )
        {
            p_vout->output.i_chroma = VLC_FOURCC('Y','V','1','2');
            p_vout->p_sys->p_overlay =
                SDL_CreateYUVOverlay( 32, 32, SDL_YV12_OVERLAY,
                                      p_vout->p_sys->p_display );
        }

        if( p_vout->p_sys->p_overlay == NULL )
        {
            p_vout->output.i_chroma = VLC_FOURCC('Y','U','Y','2');
            p_vout->p_sys->p_overlay =
                SDL_CreateYUVOverlay( 32, 32, SDL_YUY2_OVERLAY,
                                      p_vout->p_sys->p_display );
        }
    }

    if( p_vout->p_sys->p_overlay == NULL )
    {
        if( b_overlay )
            msg_Warn( p_vout, "no SDL overlay for 0x%.8x (%4.4s)",
                      p_vout->render.i_chroma,
                      (char*)&p_vout->render.i_chroma );
        else
            msg_Warn( p_vout, "SDL overlay disabled by the user" );

        switch( p_vout->p_sys->p_display->format->BitsPerPixel )
        {
            case 8:
                p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
                p_vout->output.pf_setpalette = SetPalette;
                break;
            case 15:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5');
                break;
            case 16:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
                break;
            case 24:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4');
                break;
            case 32:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
                break;
            default:
                msg_Err( p_vout, "unknown screen depth %i",
                         p_vout->p_sys->p_display->format->BitsPerPixel );
                SDL_UnlockSurface( p_vout->p_sys->p_display );
                SDL_FreeSurface( p_vout->p_sys->p_display );
                return VLC_EGENERIC;
        }

        p_vout->output.i_rmask = p_vout->p_sys->p_display->format->Rmask;
        p_vout->output.i_gmask = p_vout->p_sys->p_display->format->Gmask;
        p_vout->output.i_bmask = p_vout->p_sys->p_display->format->Bmask;

        SDL_WM_SetCaption( VOUT_TITLE " (software RGB SDL output)",
                           VOUT_TITLE " (software RGB SDL output)" );
    }
    else
    {
        if( p_vout->p_sys->p_overlay->hw_overlay )
        {
            SDL_WM_SetCaption( VOUT_TITLE " (hardware YUV SDL output)",
                               VOUT_TITLE " (hardware YUV SDL output)" );
        }
        else
        {
            SDL_WM_SetCaption( VOUT_TITLE " (software YUV SDL output)",
                               VOUT_TITLE " (software YUV SDL output)" );
        }
    }

    SDL_EventState( SDL_KEYUP, SDL_IGNORE );               /* ignore keys up */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDisplay: close and reset SDL device
 *****************************************************************************
 * This function returns all resources allocated by OpenDisplay and restore
 * the original state of the device.
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    SDL_FreeYUVOverlay( p_vout->p_sys->p_overlay );
    SDL_UnlockSurface ( p_vout->p_sys->p_display );
    SDL_FreeSurface( p_vout->p_sys->p_display );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width  = p_vout->output.i_width;
    int i_height = p_vout->output.i_height;

    if( p_vout->p_sys->p_overlay == NULL )
    {
        /* RGB picture */
        if( p_vout->p_sys->i_surfaces )
        {
            /* We already allocated this surface, return */
            return VLC_EGENERIC;
        }

        p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

        if( p_pic->p_sys == NULL )
        {
            return VLC_ENOMEM;
        }

        switch( p_vout->p_sys->p_display->format->BitsPerPixel )
        {
            case 8:
                p_pic->p->i_pixel_pitch = 1;
                break;
            case 15:
            case 16:
                p_pic->p->i_pixel_pitch = 2;
                break;
            case 24:
            case 32:
                p_pic->p->i_pixel_pitch = 4;
                break;
            default:
                return VLC_EGENERIC;
        }

        p_pic->p->p_pixels = p_vout->p_sys->p_display->pixels;
        p_pic->p->i_lines = p_vout->p_sys->p_display->h;
        p_pic->p->i_visible_lines = p_vout->p_sys->p_display->h;
        p_pic->p->i_pitch = p_vout->p_sys->p_display->pitch;
        p_pic->p->i_visible_pitch =
            p_pic->p->i_pixel_pitch * p_vout->p_sys->p_display->w;

        p_vout->p_sys->i_surfaces++;

        p_pic->i_planes = 1;
    }
    else
    {
        p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

        if( p_pic->p_sys == NULL )
        {
            return VLC_ENOMEM;
        }

        p_pic->p_sys->p_overlay =
            SDL_CreateYUVOverlay( i_width, i_height,
                                  p_vout->output.i_chroma,
                                  p_vout->p_sys->p_display );

        if( p_pic->p_sys->p_overlay == NULL )
        {
            free( p_pic->p_sys );
            return VLC_EGENERIC;
        }

        SDL_LockYUVOverlay( p_pic->p_sys->p_overlay );

        p_pic->Y_PIXELS = p_pic->p_sys->p_overlay->pixels[0];
        p_pic->p[Y_PLANE].i_lines = p_pic->p_sys->p_overlay->h;
        p_pic->p[Y_PLANE].i_visible_lines = p_pic->p_sys->p_overlay->h;
        p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[0];

        switch( p_vout->output.i_chroma )
        {
        case SDL_YV12_OVERLAY:
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w;

            p_pic->U_PIXELS = p_pic->p_sys->p_overlay->pixels[2];
            p_pic->p[U_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[U_PLANE].i_visible_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[2];
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w / 2;

            p_pic->V_PIXELS = p_pic->p_sys->p_overlay->pixels[1];
            p_pic->p[V_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[V_PLANE].i_visible_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[1];
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w / 2;

            p_pic->i_planes = 3;
            break;

        case SDL_IYUV_OVERLAY:
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w;

            p_pic->U_PIXELS = p_pic->p_sys->p_overlay->pixels[1];
            p_pic->p[U_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[U_PLANE].i_visible_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[1];
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w / 2;

            p_pic->V_PIXELS = p_pic->p_sys->p_overlay->pixels[2];
            p_pic->p[V_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[V_PLANE].i_visible_lines = p_pic->p_sys->p_overlay->h / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[2];
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w / 2;

            p_pic->i_planes = 3;
            break;

        default:
            p_pic->p[Y_PLANE].i_pixel_pitch = 2;
            p_pic->p[U_PLANE].i_visible_pitch = p_pic->p_sys->p_overlay->w * 2;

            p_pic->i_planes = 1;
            break;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    SDL_Color colors[256];
    int i;

    /* Fill colors with color information */
    for( i = 0; i < 256; i++ )
    {
        colors[ i ].r = red[ i ] >> 8;
        colors[ i ].g = green[ i ] >> 8;
        colors[ i ].b = blue[ i ] >> 8;
    }

    /* Set palette */
    if( SDL_SetColors( p_vout->p_sys->p_display, colors, 0, 256 ) == 0 )
    {
        msg_Err( p_vout, "failed to set palette" );
    }
}

