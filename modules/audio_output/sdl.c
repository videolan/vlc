/*****************************************************************************
 * sdl.c : SDL audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 VideoLAN
 * $Id: sdl.c,v 1.6 2002/08/25 09:40:00 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Pierre Baillet <oct@zoy.org>
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
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

#include SDL_INCLUDE_FILE

#define FRAME_SIZE 2048

/*****************************************************************************
 * aout_sys_t: SDL audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{   
    mtime_t call_time;
    mtime_t buffer_time;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

static int  SetFormat   ( aout_instance_t * );
static void Play        ( aout_instance_t * );

static void SDLCallback ( void *, Uint8 *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Simple DirectMedia Layer audio module") );
    set_capability( "audio output", 40 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    aout_sys_t * p_sys;

    Uint32 i_flags = SDL_INIT_AUDIO;

    if( SDL_WasInit( i_flags ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

#ifndef WIN32
    /* Win32 SDL implementation doesn't support SDL_INIT_EVENTTHREAD yet*/
    i_flags |= SDL_INIT_EVENTTHREAD;
#endif
#ifdef DEBUG
    /* In debug mode you may want vlc to dump a core instead of staying
     * stuck */
    i_flags |= SDL_INIT_NOPARACHUTE;
#endif

    /* Initialize library */
    if( SDL_Init( i_flags ) < 0 )
    {
        msg_Err( p_aout, "cannot initialize SDL (%s)", SDL_GetError() );
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetFormat: reset the audio device and sets its format
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
    aout_sys_t * p_sys = p_aout->output.p_sys;

    /* TODO: finish and clean this */
    SDL_AudioSpec desired;

    desired.freq       = p_aout->output.output.i_rate;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = p_aout->output.output.i_channels;
    desired.callback   = SDLCallback;
    desired.userdata   = p_aout;
    desired.samples    = FRAME_SIZE;

    /* Open the sound device - FIXME : get the "natural" parameters */
    if( SDL_OpenAudio( &desired, NULL ) < 0 )
    {
        return VLC_EGENERIC;
    }

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    p_aout->output.i_nb_samples = FRAME_SIZE;

    p_sys->call_time = 0;
    p_sys->buffer_time = (mtime_t)FRAME_SIZE * 1000000
                          / p_aout->output.output.i_rate;

    SDL_PauseAudio( 0 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    aout_sys_t * p_sys = p_aout->output.p_sys;

    SDL_PauseAudio( 1 );
    SDL_CloseAudio();
    SDL_QuitSubSystem( SDL_INIT_AUDIO );

    free( p_sys );
}

/*****************************************************************************
 * SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void SDLCallback( void * _p_aout, byte_t * p_stream, int i_len )
{
    aout_instance_t * p_aout = (aout_instance_t *)_p_aout;
    aout_sys_t * p_sys = p_aout->output.p_sys;

    aout_buffer_t * p_buffer;

    /* We try to stay around call_time + buffer_time/2. This is kludgy but
     * unavoidable because SDL is completely unable to 1. tell us about its
     * latency, and 2. call SDLCallback at regular intervals. */
    if( mdate() < p_sys->call_time + p_sys->buffer_time / 2 )
    {
        /* We can't wait too much, because SDL will be lost, and we can't
         * wait too little, because we are not sure that there will be
         * samples in the queue. */
        mwait( p_sys->call_time + p_sys->buffer_time / 4 );
        p_sys->call_time += p_sys->buffer_time;
    }
    else
    {
        p_sys->call_time = mdate() + p_sys->buffer_time / 4;
    }

    /* Tell the output we're playing samples at call_time + 2*buffer_time */
    p_buffer = aout_OutputNextBuffer( p_aout, p_sys->call_time
                                       + 2 * p_sys->buffer_time, VLC_TRUE );

    if ( i_len != FRAME_SIZE * sizeof(s16)
                    * p_aout->output.output.i_channels )
    {
        msg_Err( p_aout, "SDL doesn't know its buffer size (%d)", i_len );
    }

    if ( p_buffer != NULL )
    {
        p_aout->p_vlc->pf_memcpy( p_stream, p_buffer->p_buffer, i_len );
        aout_BufferFree( p_buffer );
    }
    else
    {
        p_aout->p_vlc->pf_memset( p_stream, 0, i_len );
    }
}

