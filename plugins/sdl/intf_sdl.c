/*****************************************************************************
 * intf_sdl.c: SDL interface plugin
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_sdl.c,v 1.32 2001/02/08 13:52:35 massiot Exp $
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>                                /* for all the SDL stuff */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "video.h"
#include "video_output.h"


#include "interface.h"
#include "intf_msg.h"
#include "keystrokes.h"

#include "main.h"

/* local prototype */
void intf_SDL_Keymap( intf_thread_t * p_intf );

/*****************************************************************************
 * intf_SDLCreate: initialize and create SDL interface
 *****************************************************************************/
int intf_SDLCreate( intf_thread_t *p_intf )
{
    /* Check that b_video is set */
    if( !p_main->b_video )
    {
        intf_ErrMsg( "error: SDL interface requires a video output thread" );
        return( 1 );
    }

    /* Spawn video output thread */
    p_intf->p_vout = vout_CreateThread( main_GetPszVariable( VOUT_DISPLAY_VAR,
                                                             NULL), 0,
                                        main_GetIntVariable( VOUT_WIDTH_VAR,
                                                         VOUT_WIDTH_DEFAULT ),
                                        main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                        VOUT_HEIGHT_DEFAULT ),
                                        NULL, 0, NULL );

    if( p_intf->p_vout == NULL )                                  /* error */
    {
        intf_ErrMsg( "error: can't create video output thread" );
        free( p_intf->p_sys );
        return( 1 );
    }
    intf_SDL_Keymap( p_intf );
    return( 0 );
}

/*****************************************************************************
 * intf_SDLDestroy: destroy interface
 *****************************************************************************/
void intf_SDLDestroy( intf_thread_t *p_intf )
{
    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
        vout_DestroyThread( p_intf->p_vout, NULL );
    }
}


/*****************************************************************************
 * intf_SDLManage: event loop
 *****************************************************************************/
void intf_SDLManage( intf_thread_t *p_intf )
{
    SDL_Event event;                                            /* SDL event */
    Uint8   i_key;
    int     i_rate;
 
    while ( SDL_PollEvent(&event) )
    {
        switch (event.type)
        {
        case SDL_VIDEORESIZE:                           /* Resizing of window */
            intf_Msg( "intf: video display resized (%dx%d)", event.resize.w
                                                           , event.resize.h ); 
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->i_width = event.resize.w;
            p_intf->p_vout->i_height = event.resize.h;
            p_intf->p_vout->i_changes |= VOUT_SIZE_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
            break;
            
        case SDL_KEYDOWN:                              /* if a key is pressed */
            i_key = event.key.keysym.sym;
               
            switch(i_key) 
            {
            case SDLK_f:                              /* switch to fullscreen */
                vlc_mutex_lock( &p_intf->p_vout->change_lock );
                p_intf->p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                vlc_mutex_unlock( &p_intf->p_vout->change_lock );
                break;
                
            case SDLK_y:                                /* switch to hard YUV */
                vlc_mutex_lock( &p_intf->p_vout->change_lock );
                p_intf->p_vout->i_changes |= VOUT_YUV_CHANGE;
                vlc_mutex_unlock( &p_intf->p_vout->change_lock );
                break; 

            /* FIXME : this is temporary */
            case SDLK_p:
                if( p_intf->p_input->stream.control.i_status == PLAYING_S )
                {
                    input_Pause( p_intf->p_input );
                }
                else
                {
                    input_Play( p_intf->p_input );
                }
                break;

            case SDLK_a:
                i_rate = p_intf->p_input->stream.control.i_rate/2;
                if ( i_rate >= MINIMAL_RATE )
                {
                    input_Forward( p_intf->p_input, i_rate );
                }
                break;

            case SDLK_z:
                i_rate = p_intf->p_input->stream.control.i_rate*2;
                if ( i_rate <= MAXIMAL_RATE )
                {
                    /* Compensation of int truncature */
                    if ( i_rate > 500 && i_rate < 1000 )
                        i_rate = 1000;
                    input_Forward( p_intf->p_input, i_rate );
                }
                break;

            case SDLK_j:
                /* Jump forwards */
                input_Seek( p_intf->p_input,
                            p_intf->p_input->stream.i_tell
                             + p_intf->p_input->stream.i_size / 20 );
                                                           /* gabuzomeu */
                break;

            case SDLK_b:
                /* Jump backwards */
                input_Seek( p_intf->p_input,
                            p_intf->p_input->stream.i_tell
                             - p_intf->p_input->stream.i_size / 20 );
                break;

            default:
                if( intf_ProcessKey( p_intf, (char )i_key ) )
                {
                   intf_DbgMsg( "unhandled key '%c' (%i)", (char)i_key, i_key );
                }
                break;
            }
            break;
            
        case SDL_MOUSEBUTTONDOWN:
            if( event.button.button == SDL_BUTTON_MIDDLE )
            {
                vlc_mutex_lock( &p_intf->p_vout->change_lock );
                p_intf->p_vout->i_changes |= VOUT_CURSOR_CHANGE;
                vlc_mutex_unlock( &p_intf->p_vout->change_lock );
            }                                       
            break;
            
        case SDL_QUIT:
            intf_ProcessKey( p_intf, SDLK_q );
            break;
       
        default:
            break;
        }
    }
}

void intf_SDL_Keymap(intf_thread_t * p_intf )
{
    /* p_intf->p_intf_getKey = intf_getKey; */
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
    intf_AssignKey(p_intf, SDLK_i,         INTF_KEY_TOGGLE_INFO, 0);
    intf_AssignKey(p_intf, SDLK_s,      INTF_KEY_TOGGLE_SCALING, 0);

}
