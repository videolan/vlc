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
    SDL_Surface *   p_display;                             /* display device */
    Uint8   *   p_buffer[2];
                                                     /* Buffers informations */
    boolean_t   b_must_acquire;           /* must be acquired before writing */
}   vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     SDLOpenDisplay   ( vout_thread_t *p_vout, 
                                  char *psz_display, void *p_data );
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
        intf_ErrMsg( "error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open and initialize device */
    
    if( SDLOpenDisplay( p_vout, psz_display, p_data ) )
    {
        intf_ErrMsg( "error: can't initialize SDL display\n" );
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
    // SDLCloseDisplay( p_vout );
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
    /* FIXME: 8bpp: change palette ?? */
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
    SDL_Overlay * screen;
    SDL_Rect    disp;
    if(1)
    {
        /* Change display frame */
        if( p_vout->p_sys->b_must_acquire )
        {
            SDL_Flip( p_vout->p_sys->p_display );
        }
        /* Swap buffers and change write frame */
        if( p_vout->p_sys->b_must_acquire )
        {
            SDL_LockSurface ( p_vout->p_sys->p_display );
        }
    } 
    else 
    {
        /*
         * p_vout->yuv.p_buffer contains the YUV buffer to render 
         */
        
        screen = SDL_CreateYUVOverlay( p_vout->i_width, p_vout->i_height , SDL_IYUV_OVERLAY, p_vout->p_sys->p_display );
        screen->pixels = p_vout->yuv.p_buffer;
        disp.x = 0;
        disp.y = 0;
        disp.w = p_vout->i_width;
        disp.h = p_vout->i_height;
        SDL_DisplayYUVOverlay( screen , &disp );
    }        
}

/* following functions are local */

/*****************************************************************************
 * SDLOpenDisplay: open and initialize SDL device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int SDLOpenDisplay( vout_thread_t *p_vout, char *psz_display, void *p_data )
{
    /* Initialize library */
    if( SDL_Init(SDL_INIT_VIDEO) < 0 )
    {
        intf_ErrMsg( "error: can't initialize SDL library: %s\n",
                     SDL_GetError() );
        return( 1 );
    }

    /* Open display 
     * TODO: Check that we can request for a DOUBLEBUF HWSURFACE display
     */
    if(psz_display != NULL && strcmp(psz_display,"fullscreen")==0)
    {
        p_vout->p_sys->p_display = SDL_SetVideoMode(p_vout->i_width, 
            p_vout->i_height, 
            15, 
            SDL_ANYFORMAT | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN );
    } else {
        p_vout->p_sys->p_display = SDL_SetVideoMode(p_vout->i_width, 
            p_vout->i_height, 
            15, 
            SDL_ANYFORMAT | SDL_HWSURFACE | SDL_DOUBLEBUF );
    }
	
    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg( "error: can't open DISPLAY default display\n" );
        return( 1 );
    }
    SDL_EventState(SDL_KEYUP , SDL_IGNORE);	/* ignore keys up */

    /* Check buffers properties */	
    p_vout->p_sys->b_must_acquire = 1;		/* always acquire */
	p_vout->p_sys->p_buffer[ 0 ] =
             p_vout->p_sys->p_display->pixels;
	
	SDL_Flip(p_vout->p_sys->p_display);
	p_vout->p_sys->p_buffer[ 1 ] =
             p_vout->p_sys->p_display->pixels;
	SDL_Flip(p_vout->p_sys->p_display);

    /* Set graphic context colors */

/*
	col_fg.r = col_fg.g = col_fg.b = -1;
    col_bg.r = col_bg.g = col_bg.b = 0;
    if( ggiSetGCForeground(p_vout->p_sys->p_display,
                           ggiMapColor(p_vout->p_sys->p_display,&col_fg)) ||
        ggiSetGCBackground(p_vout->p_sys->p_display,
                           ggiMapColor(p_vout->p_sys->p_display,&col_bg)) )
    {
        intf_ErrMsg("error: can't set colors\n");
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }
*/
		
    /* Set clipping for text */
    SDL_SetClipping(p_vout->p_sys->p_display, 0, 0,
                         p_vout->p_sys->p_display->w, 
			 p_vout->p_sys->p_display->h );


	
    /* Set thread information */
    p_vout->i_width =           p_vout->p_sys->p_display->w;
    p_vout->i_height =          p_vout->p_sys->p_display->h;

    p_vout->i_bytes_per_line = p_vout->p_sys->p_display->format->BytesPerPixel *
	    			p_vout->p_sys->p_display->w ;
		
    p_vout->i_screen_depth =    p_vout->p_sys->p_display->format->BitsPerPixel;
    p_vout->i_bytes_per_pixel = p_vout->p_sys->p_display->format->BytesPerPixel;
    p_vout->i_red_mask =        p_vout->p_sys->p_display->format->Rmask;
    p_vout->i_green_mask =      p_vout->p_sys->p_display->format->Gmask;
    p_vout->i_blue_mask =       p_vout->p_sys->p_display->format->Bmask;

    /* FIXME: palette in 8bpp ?? */
    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_vout->p_sys->p_buffer[ 0 ],
                             p_vout->p_sys->p_buffer[ 1 ] );

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
    SDL_FreeSurface( p_vout->p_sys->p_display );
    SDL_Quit();
}

