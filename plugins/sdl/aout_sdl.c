/*****************************************************************************
 * aout_sdl.c : audio sdl functions library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_sdl.c,v 1.31 2002/07/31 20:56:52 sam Exp $
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

#include SDL_INCLUDE_FILE

/*****************************************************************************
 * aout_sys_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/

/* the overflow limit is used to prevent the fifo from growing too big */
#define OVERFLOWLIMIT 100000

struct aout_sys_t
{
    byte_t  * audio_buf;
    int i_audio_end;

    vlc_bool_t b_active;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     SetFormat   ( aout_thread_t * );
static int     GetBufInfo  ( aout_thread_t *, int );
static void    Play        ( aout_thread_t *, byte_t *, int );

static void    SDLCallback ( void *, Uint8 *, int );

/*****************************************************************************
 * OpenAudio: open the audio device
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->i_fd with the file's descriptor.
 *****************************************************************************/
int E_(OpenAudio) ( vlc_object_t *p_this )
{
    aout_thread_t * p_aout = (aout_thread_t *)p_this;

    SDL_AudioSpec desired;

    if( SDL_WasInit( SDL_INIT_AUDIO ) != 0 )
    {
        return( 1 );
    }

    p_aout->pf_setformat = SetFormat;
    p_aout->pf_getbufinfo = GetBufInfo;
    p_aout->pf_play = Play;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );

    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( 1 );
    }

    /* Initialize library */
    if( SDL_Init( SDL_INIT_AUDIO
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
        msg_Err( p_aout, "cannot initialize SDL (%s)", SDL_GetError() );
        free( p_aout->p_sys );
        return( 1 );
    }

    p_aout->p_sys->i_audio_end = 0;
    p_aout->p_sys->audio_buf = malloc( OVERFLOWLIMIT );

    /* Initialize some variables */

    /* TODO: write conversion beetween AOUT_FORMAT_DEFAULT
     * AND AUDIO* from SDL. */
    desired.freq       = p_aout->i_rate;
#ifdef WORDS_BIGENDIAN
    desired.format     = AUDIO_S16MSB;                     /* stereo 16 bits */
#else
    desired.format     = AUDIO_S16LSB;                     /* stereo 16 bits */
#endif
    desired.channels   = p_aout->i_channels;
    desired.callback   = SDLCallback;
    desired.userdata   = p_aout->p_sys;
    desired.samples    = 1024;

    /* Open the sound device
     * we just ask the SDL to wrap at the good frequency if the one we
     * ask for is unavailable. This is done by setting the second parar
     * to NULL
     */
    if( SDL_OpenAudio( &desired, NULL ) < 0 )
    {
        msg_Err( p_aout, "SDL_OpenAudio failed (%s)", SDL_GetError() );
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
        free( p_aout->p_sys );
        return( -1 );
    }

    p_aout->p_sys->b_active = 1;
    SDL_PauseAudio( 0 );

    return( 0 );
}

/*****************************************************************************
 * SetFormat: reset the audio device and sets its format
 *****************************************************************************
 * This functions resets the audio device, tries to initialize the output
 * format with the value contained in the dsp structure, and if this value
 * could not be set, the default value returned by ioctl is set. It then
 * does the same for the stereo mode, and for the output rate.
 *****************************************************************************/
static int SetFormat( aout_thread_t *p_aout )
{
    /* TODO: finish and clean this */
    SDL_AudioSpec desired;

    /*i_format = p_aout->i_format;*/
    desired.freq       = p_aout->i_rate;             /* Set the output rate */
#ifdef WORDS_BIGENDIAN
    desired.format     = AUDIO_S16MSB;                    /* stereo 16 bits */
#else
    desired.format     = AUDIO_S16LSB;                    /* stereo 16 bits */
#endif
    desired.channels   = p_aout->i_channels;
    desired.callback   = SDLCallback;
    desired.userdata   = p_aout->p_sys;
    desired.samples    = 2048;

    /* Open the sound device */
    SDL_PauseAudio( 1 );
    SDL_CloseAudio();

    if( SDL_OpenAudio( &desired, NULL ) < 0 )
    {
        p_aout->p_sys->b_active = 0;
        return( -1 );
    }

    p_aout->p_sys->b_active = 1;
    SDL_PauseAudio( 0 );

    return( 0 );
}

/*****************************************************************************
 * GetBufInfo: buffer status query
 *****************************************************************************
 * returns the number of bytes in the audio buffer compared to the size of
 * i_buffer_limit...
 *****************************************************************************/
static int GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    if(i_buffer_limit > p_aout->p_sys->i_audio_end)
    {
        /* returning 0 here juste gives awful sound in the speakers :/ */
        return( i_buffer_limit );
    }
    return( p_aout->p_sys->i_audio_end - i_buffer_limit);
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    byte_t * audio_buf = p_aout->p_sys->audio_buf;

    SDL_LockAudio();                                     /* Stop callbacking */

    p_aout->p_sys->audio_buf = realloc( audio_buf,
                                        p_aout->p_sys->i_audio_end + i_size);
    memcpy( p_aout->p_sys->audio_buf + p_aout->p_sys->i_audio_end,
            buffer, i_size);

    p_aout->p_sys->i_audio_end += i_size;

    SDL_UnlockAudio();                                  /* go on callbacking */
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
void E_(CloseAudio) ( vlc_object_t *p_this )
{
    aout_thread_t * p_aout = (aout_thread_t *)p_this;

    if( p_aout->p_sys->b_active )
    {
        SDL_PauseAudio( 1 );                                  /* pause audio */

        if( p_aout->p_sys->audio_buf != NULL )  /* do we have a buffer now ? */
        {
            free( p_aout->p_sys->audio_buf );
        }
    }

    SDL_CloseAudio();

    SDL_QuitSubSystem( SDL_INIT_AUDIO );

    free( p_aout->p_sys );                              /* Close the Output. */
}

/*****************************************************************************
 * SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void SDLCallback( void *userdata, byte_t *stream, int len )
{
    aout_sys_t * p_sys = userdata;

    if( p_sys->i_audio_end > OVERFLOWLIMIT )
    {
//X        msg_Err( p_aout, "aout_SDLCallback overflowed" );

        free( p_sys->audio_buf );
        p_sys->audio_buf = NULL;

        p_sys->i_audio_end = 0;
        /* we've gone to slow, increase output freq */
    }

    /* if we are not in underrun */
    if( p_sys->i_audio_end > len )
    {
        p_sys->i_audio_end -= len;
        memcpy( stream, p_sys->audio_buf, len );
        memmove( p_sys->audio_buf, p_sys->audio_buf + len, p_sys->i_audio_end );
    }
}

