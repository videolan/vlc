/*****************************************************************************
 * sdl.c : SDL audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 VideoLAN
 * $Id: sdl.c,v 1.8 2002/08/30 22:22:24 massiot Exp $
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
    mtime_t next_date;
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

    Uint32 i_flags = SDL_INIT_AUDIO;

    if( SDL_WasInit( i_flags ) )
    {
        return VLC_EGENERIC;
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
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetFormat: reset the audio device and sets its format
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
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
    SDL_PauseAudio( 1 );
    SDL_CloseAudio();
    SDL_QuitSubSystem( SDL_INIT_AUDIO );
}

/*****************************************************************************
 * SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void SDLCallback( void * _p_aout, byte_t * p_stream, int i_len )
{
    aout_instance_t * p_aout = (aout_instance_t *)_p_aout;
    aout_buffer_t *   p_buffer;

    /* SDL is unable to call us at regular times, or tell us its current
     * hardware latency, or the buffer state. So we just pop data and throw
     * it at SDL's face. Nah. */

    vlc_mutex_lock( &p_aout->output_fifo_lock );
    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
    vlc_mutex_unlock( &p_aout->output_fifo_lock );

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

