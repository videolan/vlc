/*****************************************************************************
 * vout_sdl.c: SDL video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <SDL/SDL.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h"
/* FIXME: get rid of this */
#include "keystrokes.h"
#include "main.h"

/*****************************************************************************
 * FIXME: this file is ...                                                   *
 *                                                                           *
 *              XXX   XXX     FIXME     XXX     XXX   XXX   XXX              *
 *              XXX   XXX   XXX   XXX   XXX     XXX   XXX   XXX              *
 *              XXX   XXX   XXX         XXX       FIXME     XXX              *
 *              XXX   XXX   XXX  TODO   XXX        XXX      XXX              *
 *              XXX   XXX   XXX   XXX   XXX        XXX                       *
 *                FIXME       FIXME       FIXME    XXX      XXX              *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 * vout_sys_t: video output SDL method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the SDL specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    int i_width;
    int i_height;
    SDL_Surface *   p_display;                             /* display device */
    SDL_Overlay *   p_overlay;                             /* overlay device */
    boolean_t   b_fullscreen;
    boolean_t   b_overlay;
    boolean_t   b_cursor;
    boolean_t   b_reopen_display;
    Uint8   *   p_sdl_buf[2];                          /* Buffer information */
} vout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );
static void vout_SetPalette( p_vout_thread_t p_vout, u16 *red, u16 *green,
                             u16 *blue, u16 *transp );

static int  SDLOpenDisplay      ( vout_thread_t *p_vout );
static void SDLCloseDisplay     ( vout_thread_t *p_vout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = vout_SetPalette;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to initialize SDL and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "sdl" ) )
    {
        return( 999 );
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
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: can't create p_sys (%s)", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize library */
    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE)
            < 0 )
    {
        intf_ErrMsg( "vout error: can't initialize SDL (%s)", SDL_GetError() );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->p_sys->b_cursor = 1; /* TODO should be done with a main_GetInt.. */
    p_vout->p_sys->b_fullscreen = main_GetIntVariable( VOUT_FULLSCREEN_VAR,
                                VOUT_FULLSCREEN_DEFAULT );
    p_vout->p_sys->b_overlay = main_GetIntVariable( VOUT_OVERLAY_VAR,
                                VOUT_OVERLAY_DEFAULT );
    p_vout->p_sys->i_width = main_GetIntVariable( VOUT_WIDTH_VAR, 
                                VOUT_WIDTH_DEFAULT );
    p_vout->p_sys->i_height = main_GetIntVariable( VOUT_HEIGHT_VAR,
                                VOUT_HEIGHT_DEFAULT );

    p_vout->p_sys->p_display = NULL;
    p_vout->p_sys->p_overlay = NULL;

    if( SDLOpenDisplay(p_vout) )
    {
        intf_ErrMsg( "vout error: can't set up SDL (%s)", SDL_GetError() );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* FIXME: get rid of this ASAP, it's FUCKING UGLY */
    { intf_thread_t * p_intf = p_main->p_intf;
    intf_AssignKey(p_intf, SDLK_q,      INTF_KEY_QUIT, 0);
    intf_AssignKey(p_intf, SDLK_ESCAPE, INTF_KEY_QUIT, 0);
    /* intf_AssignKey(p_intf,3,'Q'); */
    intf_AssignKey(p_intf, SDLK_0,      INTF_KEY_SET_CHANNEL,0);
    intf_AssignKey(p_intf, SDLK_1,      INTF_KEY_SET_CHANNEL,1);
    intf_AssignKey(p_intf, SDLK_2,      INTF_KEY_SET_CHANNEL,2);
    intf_AssignKey(p_intf, SDLK_3,      INTF_KEY_SET_CHANNEL,3);
    intf_AssignKey(p_intf, SDLK_4,      INTF_KEY_SET_CHANNEL,4);
    intf_AssignKey(p_intf, SDLK_5,      INTF_KEY_SET_CHANNEL,5);
    intf_AssignKey(p_intf, SDLK_6,      INTF_KEY_SET_CHANNEL,6);
    intf_AssignKey(p_intf, SDLK_7,      INTF_KEY_SET_CHANNEL,7);
    intf_AssignKey(p_intf, SDLK_8,      INTF_KEY_SET_CHANNEL,8);
    intf_AssignKey(p_intf, SDLK_9,      INTF_KEY_SET_CHANNEL,9);
    intf_AssignKey(p_intf, SDLK_PLUS,   INTF_KEY_INC_VOLUME, 0);
    intf_AssignKey(p_intf, SDLK_MINUS,  INTF_KEY_DEC_VOLUME, 0);
    intf_AssignKey(p_intf, SDLK_m,      INTF_KEY_TOGGLE_VOLUME, 0);
    /* intf_AssignKey(p_intf,'M','M'); */
    intf_AssignKey(p_intf, SDLK_g,      INTF_KEY_DEC_GAMMA, 0);
    /* intf_AssignKey(p_intf,'G','G'); */
    intf_AssignKey(p_intf, SDLK_c,      INTF_KEY_TOGGLE_GRAYSCALE, 0);
    intf_AssignKey(p_intf, SDLK_SPACE,  INTF_KEY_TOGGLE_INTERFACE, 0);
    intf_AssignKey(p_intf, SDLK_i,      INTF_KEY_TOGGLE_INFO, 0);
    intf_AssignKey(p_intf, SDLK_s,      INTF_KEY_TOGGLE_SCALING, 0); }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize SDL video thread output method
 *****************************************************************************
 * This function initialize the SDL display device.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    /* This hack is hugly, but hey, you are, too. */

    SDL_Overlay *   p_overlay;

    p_overlay = SDL_CreateYUVOverlay( VOUT_WIDTH_DEFAULT, VOUT_HEIGHT_DEFAULT,
                                      SDL_YV12_OVERLAY, 
                                      p_vout->p_sys->p_display );
    intf_Msg( "vout: YUV acceleration %s",
              p_overlay->hw_overlay ? "activated" : "unavailable !" ); 
    p_vout->b_need_render = !p_overlay->hw_overlay;

    SDL_FreeYUVOverlay( p_overlay );

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    SDLCloseDisplay( p_vout );
    SDL_Quit();
}

/*****************************************************************************
 * vout_Destroy: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
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
    Uint8   i_key;

    /* Process events */
    while( SDL_PollEvent(&event) )
    {
        switch( event.type )
        {
        case SDL_VIDEORESIZE:                          /* Resizing of window */
            p_vout->i_width = event.resize.w;
            p_vout->i_height = event.resize.h;
            p_vout->i_changes |= VOUT_SIZE_CHANGE;
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
            case SDL_BUTTON_MIDDLE:
                p_vout->i_changes |= VOUT_CURSOR_CHANGE;
                break;
            }
            break;

        case SDL_QUIT:
            intf_ProcessKey( p_main->p_intf, SDLK_q );
            break;

        case SDL_KEYDOWN:                             /* if a key is pressed */
            i_key = event.key.keysym.sym;

            switch( i_key )
            {
            case SDLK_f:                             /* switch to fullscreen */
                p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                break;

            case SDLK_y:                               /* switch to hard YUV */
                p_vout->i_changes |= VOUT_YUV_CHANGE;
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

            default:
                if( intf_ProcessKey( p_main->p_intf, (char )i_key ) )
                {
                   intf_DbgMsg( "unhandled key '%c' (%i)", (char)i_key, i_key );                }
                break;
            }
            break;

        default:
            break;
        }
    }

    /*
     * Size Change 
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        p_vout->p_sys->i_width = p_vout->i_width;
        p_vout->p_sys->i_height = p_vout->i_height;

        /* Need to reopen display */
        SDLCloseDisplay( p_vout );
        if( SDLOpenDisplay( p_vout ) )
        {
          intf_ErrMsg( "vout error: can't reopen display after resize" );
          return( 1 );
        }
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
    }
    
    /*
     * YUV Change 
     */
    if( p_vout->i_changes & VOUT_YUV_CHANGE )
    {
        p_vout->b_need_render = ! p_vout->b_need_render;
        
        /* Need to reopen display */
        SDLCloseDisplay( p_vout );
        if( SDLOpenDisplay( p_vout ) )
        {
          intf_ErrMsg( "error: can't reopen display after YUV change" );
          return( 1 );
        }
        p_vout->i_changes &= ~VOUT_YUV_CHANGE;
    }

    /*
     * Fullscreen change
     */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        p_vout->p_sys->b_fullscreen = ! p_vout->p_sys->b_fullscreen;

        if( p_vout->p_sys->b_fullscreen )
        {
            p_vout->p_sys->b_fullscreen = 0;
            p_vout->p_sys->b_cursor = 1;
            SDL_ShowCursor( 1 );
        }
        else
        {
            p_vout->p_sys->b_fullscreen = 1;
            p_vout->p_sys->b_cursor = 0;
            SDL_ShowCursor( 0 );
        }

        SDL_WM_ToggleFullScreen(p_vout->p_sys->p_display);

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /*
     * Pointer change
     */
    if( p_vout->i_changes & VOUT_CURSOR_CHANGE )
    {
        if( p_vout->p_sys->b_cursor )
        {
            SDL_ShowCursor( 0 );
            p_vout->p_sys->b_cursor = 0;
        }
        else
        {
            SDL_ShowCursor( 1 );
            p_vout->p_sys->b_cursor = 1;
        }
        p_vout->i_changes &= ~VOUT_CURSOR_CHANGE;
    }
    
    return( 0 );
}

/*****************************************************************************
 * vout_SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void vout_SetPalette( p_vout_thread_t p_vout, u16 *red, u16 *green,
                         u16 *blue, u16 *transp)
{
     /* Create a display surface with a grayscale palette */
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
        intf_ErrMsg( "vout error: failed setting palette" );
    }

}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to the display, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    SDL_Rect    disp;
    if((p_vout->p_sys->p_display != NULL) && !p_vout->p_sys->b_reopen_display)
    {
        if( p_vout->b_need_render )
        {  
            /* Change display frame */
            SDL_Flip( p_vout->p_sys->p_display );
        }
        else
        {
        
            /*
             * p_vout->p_rendered_pic->p_y/u/v contains the YUV buffers to
             * render 
             */
            /* TODO: support for streams other than 4:2:0 */
            /* create the overlay if necessary */
            if( p_vout->p_sys->p_overlay == NULL )
            {
                p_vout->p_sys->p_overlay = SDL_CreateYUVOverlay( 
                                             p_vout->p_rendered_pic->i_width, 
                                             p_vout->p_rendered_pic->i_height,
                                             SDL_YV12_OVERLAY, 
                                             p_vout->p_sys->p_display
                                           );
                intf_Msg("vout: YUV acceleration %s",
                            p_vout->p_sys->p_overlay->hw_overlay
                            ? "activated" : "unavailable !" ); 
            }

            SDL_LockYUVOverlay(p_vout->p_sys->p_overlay);
            /* copy the data into video buffers */
            /* Y first */
            memcpy(p_vout->p_sys->p_overlay->pixels[0],
                   p_vout->p_rendered_pic->p_y,
                   p_vout->p_sys->p_overlay->h *
                   p_vout->p_sys->p_overlay->pitches[0]);
            /* then V */
            memcpy(p_vout->p_sys->p_overlay->pixels[1],
                   p_vout->p_rendered_pic->p_v,
                   p_vout->p_sys->p_overlay->h *
                   p_vout->p_sys->p_overlay->pitches[1] / 2);
            /* and U */
            memcpy(p_vout->p_sys->p_overlay->pixels[2],
                   p_vout->p_rendered_pic->p_u,
                   p_vout->p_sys->p_overlay->h *
                   p_vout->p_sys->p_overlay->pitches[2] / 2);

            disp.w = (&p_vout->p_buffer[p_vout->i_buffer_index])->i_pic_width;
            disp.h = (&p_vout->p_buffer[p_vout->i_buffer_index])->i_pic_height;
            disp.x = (p_vout->i_width - disp.w)/2;
            disp.y = (p_vout->i_height - disp.h)/2;

            SDL_DisplayYUVOverlay( p_vout->p_sys->p_overlay , &disp );
            SDL_UnlockYUVOverlay(p_vout->p_sys->p_overlay);
        }
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
    SDL_Rect    clipping_rect;
    Uint32      flags;
    int bpp;
    /* Open display 
     * TODO: Check that we can request for a DOUBLEBUF HWSURFACE display
     */

    /* init flags and cursor */
    flags = SDL_ANYFORMAT | SDL_HWPALETTE;

    if( p_vout->p_sys->b_fullscreen )
        flags |= SDL_FULLSCREEN;
    else
        flags |= SDL_RESIZABLE;

    if( p_vout->b_need_render )
        flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;
    else
        flags |= SDL_SWSURFACE; /* save video memory */

    bpp = SDL_VideoModeOK( p_vout->p_sys->i_width,
                           p_vout->p_sys->i_height,
                           p_vout->i_screen_depth, flags );

    if(bpp == 0)
    {
        intf_ErrMsg( "vout error: no video mode available" );
        return( 1 );
    }

    p_vout->p_sys->p_display = SDL_SetVideoMode(p_vout->p_sys->i_width,
                                                p_vout->p_sys->i_height,
                                                bpp, flags);

    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg( "vout error: cannot set video mode" );
        return( 1 );
    }

    SDL_LockSurface(p_vout->p_sys->p_display);

    if( p_vout->p_sys->b_fullscreen )
        SDL_ShowCursor( 0 );
    else
        SDL_ShowCursor( 1 );

    SDL_WM_SetCaption( VOUT_TITLE " (SDL output)",
                       VOUT_TITLE " (SDL output)" );
    SDL_EventState(SDL_KEYUP , SDL_IGNORE);                /* ignore keys up */

    if( p_vout->b_need_render )
    {
        p_vout->p_sys->p_sdl_buf[ 0 ] = p_vout->p_sys->p_display->pixels;
        SDL_Flip(p_vout->p_sys->p_display);
        p_vout->p_sys->p_sdl_buf[ 1 ] = p_vout->p_sys->p_display->pixels;
        SDL_Flip(p_vout->p_sys->p_display);

        /* Set clipping for text */
        clipping_rect.x = 0;
        clipping_rect.y = 0;
        clipping_rect.w = p_vout->p_sys->p_display->w;
        clipping_rect.h = p_vout->p_sys->p_display->h;
        SDL_SetClipRect(p_vout->p_sys->p_display, &clipping_rect);

        /* Set thread information */
        p_vout->i_width =           p_vout->p_sys->p_display->w;
        p_vout->i_height =          p_vout->p_sys->p_display->h;
        p_vout->i_bytes_per_line =  p_vout->p_sys->p_display->pitch;

        p_vout->i_screen_depth =
            p_vout->p_sys->p_display->format->BitsPerPixel;
        p_vout->i_bytes_per_pixel =
            p_vout->p_sys->p_display->format->BytesPerPixel;

        p_vout->i_red_mask =        p_vout->p_sys->p_display->format->Rmask;
        p_vout->i_green_mask =      p_vout->p_sys->p_display->format->Gmask;
        p_vout->i_blue_mask =       p_vout->p_sys->p_display->format->Bmask;

        /* FIXME: palette in 8bpp ?? */
        /* Set and initialize buffers */
        vout_SetBuffers( p_vout, p_vout->p_sys->p_sdl_buf[ 0 ],
                                 p_vout->p_sys->p_sdl_buf[ 1 ] );
    }
    else
    {
        p_vout->p_sys->p_sdl_buf[ 0 ] = p_vout->p_sys->p_display->pixels;
        p_vout->p_sys->p_sdl_buf[ 1 ] = p_vout->p_sys->p_display->pixels;

        /* Set thread information */
        p_vout->i_width =           p_vout->p_sys->p_display->w;
        p_vout->i_height =          p_vout->p_sys->p_display->h;
        p_vout->i_bytes_per_line =  p_vout->p_sys->p_display->pitch;

        vout_SetBuffers( p_vout, p_vout->p_sys->p_sdl_buf[ 0 ],
                                 p_vout->p_sys->p_sdl_buf[ 1 ] );
    }

    p_vout->p_sys->b_reopen_display = 0;

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
    if( p_vout->p_sys->p_display != NULL )
    {
        if( p_vout->p_sys->p_overlay != NULL )
        {            
            SDL_FreeYUVOverlay( p_vout->p_sys->p_overlay );
            p_vout->p_sys->p_overlay = NULL;
        }

        SDL_UnlockSurface ( p_vout->p_sys->p_display );
        SDL_FreeSurface( p_vout->p_sys->p_display );
        p_vout->p_sys->p_display = NULL;
    }
}

