/*****************************************************************************
 * vout_sdl.c: SDL video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors:
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
#include "plugins.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"

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
    boolean_t   b_reopen_display;
    Uint8   *   p_buffer[2];
                                                     /* Buffers informations */
    boolean_t   b_must_acquire;           /* must be acquired before writing */
}   vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     SDLOpenDisplay   ( vout_thread_t *p_vout );
static void    SDLCloseDisplay  ( vout_thread_t *p_vout );

/*****************************************************************************
 * vout_SDLCreate: allocate SDL video thread output method
 *****************************************************************************
 * This function allocate and initialize a SDL vout method. It uses some of the
 * vout properties to choose the correct mode, and change them according to the
 * mode actually used.
 *****************************************************************************/
int vout_SDLCreate( vout_thread_t *p_vout, char *psz_display,
                    int i_root_window, void *p_data )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    p_vout->p_sys->p_display = NULL;
    p_vout->p_sys->p_overlay = NULL;
    p_vout->p_sys->b_must_acquire = 0;

    /* Initialize library */
    if( SDL_Init(SDL_INIT_VIDEO) < 0 )
    {
        intf_ErrMsg( "error: can't initialize SDL library: %s",
                     SDL_GetError() );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Force the software yuv even if it is not used */
    /* If we don't do this, p_vout is not correctly initialized
       and it's impossible to switch between soft/hard yuv */
    p_vout->b_need_render = 1;

    if(psz_display != NULL && strcmp(psz_display,"fullscreen")==0)
    {
        p_vout->p_sys->b_fullscreen = 1;
    }
    else
    {
        p_vout->p_sys->b_fullscreen = 0;
    }

    p_vout->p_sys->i_width = VOUT_WIDTH_DEFAULT;
    p_vout->p_sys->i_height = VOUT_HEIGHT_DEFAULT;

    p_vout->p_sys->b_reopen_display = 0;

    if( SDLOpenDisplay(p_vout) )
    {
        intf_ErrMsg( "error: can't initialize SDL library: %s",
                     SDL_GetError() );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_SDLInit: initialize SDL video thread output method
 *****************************************************************************
 * This function initialize the SDL display device.
 *****************************************************************************/
int vout_SDLInit( vout_thread_t *p_vout )
{
    /* Acquire first buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        SDL_LockSurface(p_vout->p_sys->p_display);
    }

    return( 0 );
}

/*****************************************************************************
 * vout_SDLEnd: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
void vout_SDLEnd( vout_thread_t *p_vout )
{
    /* Release buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        SDL_UnlockSurface ( p_vout->p_sys->p_display );
    }
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_SDLDestroy: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_SDLCreate
 *****************************************************************************/
void vout_SDLDestroy( vout_thread_t *p_vout )
{
    SDLCloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_SDLManage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occured.
 *****************************************************************************/
int vout_SDLManage( vout_thread_t *p_vout )
{
    /* If the display has to be reopened we do so */
    if( p_vout->p_sys->b_reopen_display )
    {
        p_vout->p_sys->b_must_acquire = 0;
        if( p_vout->p_sys->p_display )
        {
            if( p_vout->p_sys->p_overlay )
            {
                SDL_FreeYUVOverlay(p_vout->p_sys->p_overlay);
                p_vout->p_sys->p_overlay = NULL;
            }
            SDL_FreeSurface( p_vout->p_sys->p_display );
            p_vout->p_sys->p_display = NULL;
        }

        if( SDLOpenDisplay(p_vout) )
        {
            intf_ErrMsg( "error: can't open DISPLAY default display" );
            return( 1 );
        }
        p_vout->p_sys->b_reopen_display = 0;
    }
    return( 0 );
}

/*****************************************************************************
 * vout_SDLDisplay: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to the display, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
void vout_SDLDisplay( vout_thread_t *p_vout )
{
    SDL_Rect    disp;
    if(p_vout->b_need_render)
    {  
        /* Change display frame */
        if( p_vout->p_sys->b_must_acquire )
        {
            
            SDL_Flip( p_vout->p_sys->p_display );
            
            /* Swap buffers and change write frame */
            SDL_LockSurface ( p_vout->p_sys->p_display );
        }
     }
     else
     {
        
        /*
         * p_vout->p_rendered_pic->p_y/u/v contains the YUV buffers to render 
         */
        if( p_vout->p_sys->b_must_acquire )
        {
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
                intf_Msg("[YUV acceleration] : %d,",
                            p_vout->p_sys->p_overlay->hw_overlay); 
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
    flags = SDL_ANYFORMAT;

    if( p_vout->p_sys->b_fullscreen )
        flags |= SDL_FULLSCREEN;
    else
        flags |= SDL_RESIZABLE;

    if( p_vout->b_need_render )
        flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;
    else
        flags |= SDL_SWSURFACE; /* save video memory */

    bpp = SDL_VideoModeOK(p_vout->p_sys->i_width,
                          p_vout->p_sys->i_height,
                          p_vout->i_screen_depth, flags);

    if(bpp == 0)
    {
        intf_ErrMsg( "error: can't open DISPLAY default display" );
        return( 1 );
    }

    p_vout->p_sys->p_display = SDL_SetVideoMode(p_vout->p_sys->i_width,
                                                p_vout->p_sys->i_height,
                                                bpp, flags);

    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg( "error: can't open DISPLAY default display" );
        return( 1 );
    }

    if( p_vout->p_sys->b_fullscreen )
        SDL_ShowCursor( 0 );
    else
        SDL_ShowCursor( 1 );

    SDL_WM_SetCaption( VOUT_TITLE , VOUT_TITLE );
    SDL_EventState(SDL_KEYUP , SDL_IGNORE); /* ignore keys up */

    if( p_vout->b_need_render )
    {
        p_vout->p_sys->p_buffer[ 0 ] = p_vout->p_sys->p_display->pixels;
        SDL_Flip(p_vout->p_sys->p_display);
        p_vout->p_sys->p_buffer[ 1 ] = p_vout->p_sys->p_display->pixels;
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
        vout_SetBuffers( p_vout, p_vout->p_sys->p_buffer[ 0 ],
                                 p_vout->p_sys->p_buffer[ 1 ] );
    }
    else
    {
        p_vout->p_sys->p_buffer[ 0 ] = p_vout->p_sys->p_display->pixels;
        p_vout->p_sys->p_buffer[ 1 ] = p_vout->p_sys->p_display->pixels;

        /* Set thread information */
        p_vout->i_width =           p_vout->p_sys->p_display->w;
        p_vout->i_height =          p_vout->p_sys->p_display->h;
        p_vout->i_bytes_per_line =  p_vout->p_sys->p_display->pitch;

        vout_SetBuffers( p_vout, p_vout->p_sys->p_buffer[ 0 ],
                                 p_vout->p_sys->p_buffer[ 1 ] );
    }

    p_vout->i_changes |= VOUT_YUV_CHANGE;

    /* Check buffers properties */
    p_vout->p_sys->b_must_acquire = 1; /* always acquire */

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
    if( p_vout->p_sys->p_display )
    {
        p_vout->p_sys->b_must_acquire = 0;
        if( p_vout->p_sys->p_overlay )
        {
            SDL_FreeYUVOverlay(p_vout->p_sys->p_overlay);
            p_vout->p_sys->p_overlay = NULL;
        }
        SDL_UnlockSurface(p_vout->p_sys->p_display);
        SDL_FreeSurface( p_vout->p_sys->p_display );
        p_vout->p_sys->p_display = NULL;
    }

    SDL_Quit();
}

