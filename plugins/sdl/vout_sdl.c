/*****************************************************************************
 * vout_sdl.c: SDL video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_sdl.c,v 1.76 2002/01/04 14:01:34 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include <sys/types.h>
#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#include SDL_INCLUDE_FILE

#include "netutils.h"

#include "video.h"
#include "video_output.h"

#include "interface.h"

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"

#define SDL_MAX_DIRECTBUFFERS 5
#define SDL_DEFAULT_BPP 16

/*****************************************************************************
 * vout_sys_t: video output SDL method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the SDL specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    SDL_Surface *   p_display;                             /* display device */
    SDL_Overlay *   p_overlay; /* An overlay we keep to grab the XVideo port */

    int i_width;
    int i_height;

    /* For YUV output */
    u32 i_chroma;                          /* The internally selected chroma */

    /* For RGB output */
    int i_surfaces;
    int i_red_mask;
    int i_green_mask;
    int i_blue_mask;

    boolean_t   b_cursor;
    boolean_t   b_cursor_autohidden;
    mtime_t     i_lastmoved;

} vout_sys_t;

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * SDL specific properties of a direct buffer.
 *****************************************************************************/
typedef struct picture_sys_s
{
    SDL_Overlay *p_overlay;

} picture_sys_t;

/*****************************************************************************
 * Seeking function TODO: put this in a generic location !
 *****************************************************************************/
static __inline__ void vout_Seek( off_t i_seek )
{
#define area p_main->p_intf->p_input->stream.p_selected_area
    off_t i_tell = area->i_tell;

    i_tell += i_seek * (off_t)50 * p_main->p_intf->p_input->stream.i_mux_rate;

    i_tell = ( i_tell <= area->i_start ) ? area->i_start
           : ( i_tell >= area->i_size ) ? area->i_size
           : i_tell;

    input_Seek( p_main->p_intf->p_input, i_tell );
#undef area
}

/*****************************************************************************
 * Return the best suited FourCC value for a given chroma. We use this
 * because a decoder may output FOURCC_IYUV, which is exactly the same as
 * FOURCC_I420, but X servers usually know FOURCC_I420 and not FOURCC_IYUV.
 *****************************************************************************/
static __inline__ u32 BestChroma( u32 i_chroma )
{
    /* XXX: don't forget to update vout_Init if you change this */
    switch( i_chroma )
    {
        /* These ones are almost the same */
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:
            return SDL_YV12_OVERLAY;

        /* These ones are all the same */
        case FOURCC_UYVY:
        case FOURCC_UYNV:
        case FOURCC_Y422:
            return SDL_UYVY_OVERLAY;

        /* These ones are all the same */
        case FOURCC_YUY2:
        case FOURCC_YUNV:
            return SDL_YUY2_OVERLAY;

        /* We know this one */
        case FOURCC_YVYU:
            return SDL_YVYU_OVERLAY;

        /* This is seldom supported, but we know how to convert it */
        case FOURCC_I422:
            return SDL_YUY2_OVERLAY;

        /* We don't know this chroma, but maybe SDL does */
        default:
            return i_chroma;
    }
}

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  vout_Probe      ( probedata_t *p_data );
static int  vout_Create     ( struct vout_thread_s * );
static int  vout_Init       ( struct vout_thread_s * );
static void vout_End        ( struct vout_thread_s * );
static void vout_Destroy    ( struct vout_thread_s * );
static int  vout_Manage     ( struct vout_thread_s * );
static void vout_Render     ( struct vout_thread_s *, struct picture_s * );
static void vout_Display    ( struct vout_thread_s *, struct picture_s * );

static int  SDLOpenDisplay      ( vout_thread_t *p_vout );
static void SDLCloseDisplay     ( vout_thread_t *p_vout );
static int  SDLNewPicture       ( vout_thread_t *p_vout, picture_t *p_pic );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to initialize SDL and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( SDL_WasInit( SDL_INIT_VIDEO ) != 0 )
    {
        return( 0 );
    }

    return( 100 );
}

/*****************************************************************************
 * vout_Create: allocate SDL video thread output method
 *****************************************************************************
 * This function allocate and initialize a SDL vout method. It uses some of the
 * vout properties to choose the correct mode, and change them according to the
 * mode actually used.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    if( SDL_WasInit( SDL_INIT_VIDEO ) != 0 )
    {
        return( 1 );
    }

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: can't create p_sys (%s)", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize library */
    if( SDL_Init( SDL_INIT_VIDEO
#ifndef WIN32
    /* Win32 SDL implementation doesn't support SDL_INIT_EVENTTHREAD yet*/
                | SDL_INIT_EVENTTHREAD
#endif
#ifdef DEBUG
    /* In debug mode you may want vlc to dump a core instead of staying
     * stuck */
                | SDL_INIT_NOPARACHUTE
#endif
                ) < 0 )
    {
        intf_ErrMsg( "vout error: can't initialize SDL (%s)", SDL_GetError() );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->p_sys->b_cursor = 1;
    p_vout->p_sys->b_cursor_autohidden = 0;
    p_vout->p_sys->i_lastmoved = mdate();

    if( p_vout->render.i_height * p_vout->render.i_aspect
         >= p_vout->render.i_width * VOUT_ASPECT_FACTOR )
    {
        p_vout->p_sys->i_width = p_vout->render.i_height
            * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
        p_vout->p_sys->i_height = p_vout->render.i_height;
    }
    else
    {
        p_vout->p_sys->i_width = p_vout->render.i_width;
        p_vout->p_sys->i_height = p_vout->render.i_width
            * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }

#if 0
    if( p_vout->p_sys->i_width <= 300 && p_vout->p_sys->i_height <= 300 )
    {
        p_vout->p_sys->i_width <<= 1;
        p_vout->p_sys->i_height <<= 1;
    }
    else if( p_vout->p_sys->i_width <= 400 && p_vout->p_sys->i_height <= 400 )
    {
        p_vout->p_sys->i_width += p_vout->p_sys->i_width >> 1;
        p_vout->p_sys->i_height += p_vout->p_sys->i_height >> 1;
    }
#endif

    if( SDLOpenDisplay( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't set up SDL (%s)", SDL_GetError() );
        SDL_QuitSubSystem( SDL_INIT_VIDEO );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize SDL video thread output method
 *****************************************************************************
 * This function initialize the SDL display device.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    p_vout->p_sys->i_surfaces = 0;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    if( p_vout->p_sys->p_overlay == NULL )
    {
        /* All we have is an RGB image with square pixels */
        p_vout->output.i_chroma = p_vout->p_sys->i_chroma;
        p_vout->output.i_width  = p_vout->p_sys->i_width;
        p_vout->output.i_height = p_vout->p_sys->i_height;
        p_vout->output.i_aspect = p_vout->p_sys->i_width
                                   * VOUT_ASPECT_FACTOR
                                   / p_vout->p_sys->i_height;
    }
    else
    {
        switch( p_vout->render.i_chroma )
        {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:

        case FOURCC_UYVY:
        case FOURCC_UYNV:
        case FOURCC_Y422:

        case FOURCC_YUY2:
        case FOURCC_YUNV:

        case FOURCC_YVYU:
            /* We can directly handle all these chromas */
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        case FOURCC_I422:
            /* We need to convert this one, but at least we keep the
             * aspect ratio */
            p_vout->output.i_chroma = p_vout->p_sys->i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            /* All we have is an RGB image with square pixels */
            p_vout->output.i_chroma = p_vout->p_sys->i_chroma;
            p_vout->output.i_width  = p_vout->p_sys->i_width;
            p_vout->output.i_height = p_vout->p_sys->i_height;
            p_vout->output.i_aspect = p_vout->p_sys->i_width
                                       * VOUT_ASPECT_FACTOR
                                       / p_vout->p_sys->i_height;
            break;
        }
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
        if( p_pic == NULL || SDLNewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
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
 * vout_Destroy: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    SDLCloseDisplay( p_vout );

    SDL_QuitSubSystem( SDL_INIT_VIDEO );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occured.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    SDL_Event event;                                            /* SDL event */

    /* Process events */
    while( SDL_PollEvent(&event) )
    {
        switch( event.type )
        {
        case SDL_VIDEORESIZE:                          /* Resizing of window */
            p_vout->p_sys->i_width = event.resize.w;
            p_vout->p_sys->i_height = event.resize.h;
            SDLCloseDisplay( p_vout );
            SDLOpenDisplay( p_vout );
            break;

        case SDL_MOUSEMOTION:
            if( p_vout->p_sys->b_cursor &&
                (abs(event.motion.xrel) > 2 || abs(event.motion.yrel) > 2) )
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

        case SDL_MOUSEBUTTONUP:
            switch( event.button.button )
            {
            case SDL_BUTTON_RIGHT:
                p_main->p_intf->b_menu_change = 1;
                break;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            switch( event.button.button )
            {
            case SDL_BUTTON_LEFT:
                /* In this part we will eventually manage
                 * clicks for DVD navigation for instance. For the
                 * moment just pause the stream. */
                input_SetStatus( p_main->p_intf->p_input, INPUT_STATUS_PAUSE );
                break;

            case 4:
                vout_Seek( 15 );
                break;

            case 5:
                vout_Seek( -15 );
                break;
            }
            break;

        case SDL_QUIT:
            p_main->p_intf->b_die = 1;
            break;

        case SDL_KEYDOWN:                             /* if a key is pressed */

            switch( event.key.keysym.sym )
            {
            case SDLK_q:                                             /* quit */
            case SDLK_ESCAPE:
                p_main->p_intf->b_die = 1;
                break;

            case SDLK_f:                             /* switch to fullscreen */
                p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                break;

            case SDLK_c:                                 /* toggle grayscale */
                p_vout->b_grayscale = ! p_vout->b_grayscale;
                p_vout->i_changes |= VOUT_GRAYSCALE_CHANGE;
                break;

            case SDLK_i:                                      /* toggle info */
                p_vout->b_info = ! p_vout->b_info;
                p_vout->i_changes |= VOUT_INFO_CHANGE;
                break;

            case SDLK_s:                                   /* toggle scaling */
                p_vout->b_scale = ! p_vout->b_scale;
                p_vout->i_changes |= VOUT_SCALE_CHANGE;
                break;

            case SDLK_SPACE:                             /* toggle interface */
                p_vout->b_interface = ! p_vout->b_interface;
                p_vout->i_changes |= VOUT_INTF_CHANGE;
                break;
            
            case SDLK_MENU:
                p_main->p_intf->b_menu_change = 1;
                break;

            case SDLK_LEFT:
                vout_Seek( -5 );
                break;

            case SDLK_RIGHT:
                vout_Seek( 5 );
                break;

            case SDLK_UP:
                vout_Seek( 60 );
                break;

            case SDLK_DOWN:
                vout_Seek( -60 );
                break;

            case SDLK_F10: network_ChannelJoin( 0 ); break;
            case SDLK_F1:  network_ChannelJoin( 1 ); break;
            case SDLK_F2:  network_ChannelJoin( 2 ); break;
            case SDLK_F3:  network_ChannelJoin( 3 ); break;
            case SDLK_F4:  network_ChannelJoin( 4 ); break;
            case SDLK_F5:  network_ChannelJoin( 5 ); break;
            case SDLK_F6:  network_ChannelJoin( 6 ); break;
            case SDLK_F7:  network_ChannelJoin( 7 ); break;
            case SDLK_F8:  network_ChannelJoin( 8 ); break;
            case SDLK_F9:  network_ChannelJoin( 9 ); break;

            default:
                intf_DbgMsg( "unhandled key %i", event.key.keysym.sym );
                break;
            }
            break;

        default:
            break;
        }
    }

    /* Fullscreen change */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        p_vout->b_fullscreen = ! p_vout->b_fullscreen;

        SDL_WM_ToggleFullScreen(p_vout->p_sys->p_display);

        p_vout->p_sys->b_cursor_autohidden = 0;
        SDL_ShowCursor( p_vout->p_sys->b_cursor &&
                        ! p_vout->p_sys->b_cursor_autohidden );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /* Pointer change */
    if( ! p_vout->p_sys->b_cursor_autohidden &&
        ( mdate() - p_vout->p_sys->i_lastmoved > 2000000 ) )
    {
        /* Hide the mouse automatically */
        p_vout->p_sys->b_cursor_autohidden = 1;
        SDL_ShowCursor( 0 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Render: render previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the display.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    int x, y, w, h;
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
 * SDLOpenDisplay: open and initialize SDL device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int SDLOpenDisplay( vout_thread_t *p_vout )
{
    Uint32 i_flags;
    int    i_bpp;

    /* Initialize flags and cursor */
    i_flags = SDL_ANYFORMAT | SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF;
    i_flags |= p_vout->b_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;

    i_bpp = SDL_VideoModeOK( p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                             SDL_DEFAULT_BPP, i_flags );
    if( i_bpp == 0 )
    {
        intf_ErrMsg( "vout error: no video mode available" );
        return( 1 );
    }

    p_vout->p_sys->p_display = SDL_SetVideoMode( p_vout->p_sys->i_width,
                                                 p_vout->p_sys->i_height,
                                                 i_bpp, i_flags );

    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg( "vout error: cannot set video mode" );
        return( 1 );
    }

    SDL_LockSurface( p_vout->p_sys->p_display );

    /* Ask BestChroma what we should use as a chroma */
    p_vout->p_sys->i_chroma = BestChroma( p_vout->render.i_chroma );

    p_vout->p_sys->p_overlay =
        SDL_CreateYUVOverlay( 32, 32, p_vout->p_sys->i_chroma,
                              p_vout->p_sys->p_display );

    /* See if BestChroma's guess was valid */
    if( p_vout->p_sys->p_overlay == NULL )
    {
        intf_WarnMsg( 3, "vout warning: cannot set SDL overlay 0x%.8llx",
                         p_vout->p_sys->i_chroma );

        switch( p_vout->p_sys->p_display->format->BitsPerPixel )
        {
            case 8:
                p_vout->p_sys->i_chroma = FOURCC_BI_RGB;
                break;
            case 15:
                p_vout->p_sys->i_chroma = FOURCC_RV15;
                break;
            case 16:
                p_vout->p_sys->i_chroma = FOURCC_RV16;
                break;
            case 24:
                p_vout->p_sys->i_chroma = FOURCC_BI_BITFIELDS;
                break;
            case 32:
                p_vout->p_sys->i_chroma = FOURCC_BI_BITFIELDS;
                break;
            default:
                intf_ErrMsg( "vout error: unknown screen depth" );
                SDL_UnlockSurface( p_vout->p_sys->p_display );
                SDL_FreeSurface( p_vout->p_sys->p_display );
                return( -1 );
        }

        p_vout->p_sys->i_red_mask = p_vout->p_sys->p_display->format->Rmask;
        p_vout->p_sys->i_green_mask = p_vout->p_sys->p_display->format->Gmask;
        p_vout->p_sys->i_blue_mask = p_vout->p_sys->p_display->format->Bmask;

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

    return( 0 );
}

/*****************************************************************************
 * SDLCloseDisplay: close and reset SDL device
 *****************************************************************************
 * This function returns all resources allocated by SDLOpenDisplay and restore
 * the original state of the device.
 *****************************************************************************/
static void SDLCloseDisplay( vout_thread_t *p_vout )
{
    SDL_FreeYUVOverlay( p_vout->p_sys->p_overlay );
    SDL_UnlockSurface ( p_vout->p_sys->p_display );
    SDL_FreeSurface( p_vout->p_sys->p_display );
}

/*****************************************************************************
 * SDLNewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int SDLNewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width  = p_vout->output.i_width;
    int i_height = p_vout->output.i_height;

    switch( p_vout->p_sys->i_chroma )
    {
        case SDL_YV12_OVERLAY:
        case SDL_IYUV_OVERLAY:
        case SDL_YUY2_OVERLAY:
        case SDL_UYVY_OVERLAY:
        case SDL_YVYU_OVERLAY:
            p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

            if( p_pic->p_sys == NULL )
            {
                return -1;
            }

            p_pic->p_sys->p_overlay =
                SDL_CreateYUVOverlay( i_width, i_height,
                                      p_vout->p_sys->i_chroma,
                                      p_vout->p_sys->p_display );

            if( p_pic->p_sys->p_overlay == NULL )
            {
                free( p_pic->p_sys );
                return -1;
            }

            SDL_LockYUVOverlay( p_pic->p_sys->p_overlay );

            p_pic->Y_PIXELS = p_pic->p_sys->p_overlay->pixels[0];
            p_pic->p[Y_PLANE].i_lines = p_pic->p_sys->p_overlay->h;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[0];

            switch( p_vout->p_sys->i_chroma )
            {
            case SDL_YV12_OVERLAY:
                p_pic->p[Y_PLANE].i_pixel_bytes = 1;
                p_pic->p[Y_PLANE].b_margin = 0;

                p_pic->U_PIXELS = p_pic->p_sys->p_overlay->pixels[2];
                p_pic->p[U_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
                p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[2];
                p_pic->p[U_PLANE].i_pixel_bytes = 1;
                p_pic->p[U_PLANE].b_margin = 0;

                p_pic->V_PIXELS = p_pic->p_sys->p_overlay->pixels[1];
                p_pic->p[V_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
                p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[1];
                p_pic->p[V_PLANE].i_pixel_bytes = 1;
                p_pic->p[V_PLANE].b_margin = 0;

                p_pic->i_planes = 3;
                break;

            case SDL_IYUV_OVERLAY:
                p_pic->p[Y_PLANE].i_pixel_bytes = 1;
                p_pic->p[Y_PLANE].b_margin = 0;

                p_pic->U_PIXELS = p_pic->p_sys->p_overlay->pixels[1];
                p_pic->p[U_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
                p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[1];
                p_pic->p[U_PLANE].i_pixel_bytes = 1;
                p_pic->p[U_PLANE].b_margin = 0;

                p_pic->V_PIXELS = p_pic->p_sys->p_overlay->pixels[2];
                p_pic->p[V_PLANE].i_lines = p_pic->p_sys->p_overlay->h / 2;
                p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_overlay->pitches[2];
                p_pic->p[V_PLANE].i_pixel_bytes = 1;
                p_pic->p[V_PLANE].b_margin = 0;

                p_pic->i_planes = 3;
                break;

            default:
                p_pic->p[Y_PLANE].i_pixel_bytes = 2;
                p_pic->p[Y_PLANE].b_margin = 0;

                p_pic->i_planes = 1;
                break;
            }

            return 0;

        default:
            /* RGB picture */
            if( p_vout->p_sys->i_surfaces )
            {
                /* We already allocated this surface, return */
                return -1;
            }

            p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

            if( p_pic->p_sys == NULL )
            {
                return -1;
            }

            p_pic->p->p_pixels = p_vout->p_sys->p_display->pixels;
            p_pic->p->i_lines = p_vout->p_sys->p_display->h;
            p_pic->p->i_pitch = p_vout->p_sys->p_display->pitch;

            p_pic->p->i_pixel_bytes = 2;
            if( p_pic->p->i_pitch != 2 * p_vout->p_sys->p_display->w )
            {
              intf_ErrMsg("OOO XXX OOO --- Wooooooohoooo !! --- OOO XXX OOO");
              intf_ErrMsg("%i != 2 * %i", p_pic->p->i_pitch, p_vout->p_sys->p_display->w );
            }
            p_pic->p->b_margin = 0;

            p_pic->p->i_red_mask = p_vout->p_sys->p_display->format->Rmask;
            p_pic->p->i_green_mask = p_vout->p_sys->p_display->format->Gmask;
            p_pic->p->i_blue_mask = p_vout->p_sys->p_display->format->Bmask;

            p_vout->p_sys->i_surfaces++;

            p_pic->i_planes = 1;

            return 0;
    }
}

