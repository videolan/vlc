/*****************************************************************************
 * aout_sdl.c : audio sdl functions library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_sdl.c,v 1.27.2.1 2002/05/31 21:57:51 massiot Exp $
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <videolan/vlc.h>

#include SDL_INCLUDE_FILE

#include "audio_output.h"                                   /* aout_thread_t */

/*****************************************************************************
 * aout_sys_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/

/* the overflow limit is used to prevent the fifo from growing too big */
#define OVERFLOWLIMIT 100000

typedef struct aout_sys_s
{
    byte_t  * audio_buf;
    int i_audio_end;

    boolean_t b_active;

} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static int     aout_GetBufInfo  ( aout_thread_t *p_aout, int i_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

static void    aout_SDLCallback ( void *userdata, Uint8 *stream, int len );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Open: open the audio device
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->i_fd with the file's descriptor.
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    SDL_AudioSpec desired;

    if( SDL_WasInit( SDL_INIT_AUDIO ) != 0 )
    {
        return( 1 );
    }

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );

    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg( "aout error: %s", strerror(ENOMEM) );
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
        intf_ErrMsg( "aout error: can't initialize SDL (%s)", SDL_GetError() );
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
    desired.callback   = aout_SDLCallback;
    desired.userdata   = p_aout->p_sys;
    desired.samples    = 1024;

    /* Open the sound device
     * we just ask the SDL to wrap at the good frequency if the one we
     * ask for is unavailable. This is done by setting the second parar
     * to NULL
     */
    if( SDL_OpenAudio( &desired, NULL ) < 0 )
    {
        intf_ErrMsg( "aout error: SDL_OpenAudio failed (%s)", SDL_GetError() );
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
        free( p_aout->p_sys );
        return( -1 );
    }

    p_aout->p_sys->b_active = 1;
    SDL_PauseAudio( 0 );

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: reset the audio device and sets its format
 *****************************************************************************
 * This functions resets the audio device, tries to initialize the output
 * format with the value contained in the dsp structure, and if this value
 * could not be set, the default value returned by ioctl is set. It then
 * does the same for the stereo mode, and for the output rate.
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
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
    desired.callback   = aout_SDLCallback;
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
 * aout_GetBufInfo: buffer status query
 *****************************************************************************
 * returns the number of bytes in the audio buffer compared to the size of
 * i_buffer_limit...
 *****************************************************************************/
static int aout_GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    if(i_buffer_limit > p_aout->p_sys->i_audio_end)
    {
        /* returning 0 here juste gives awful sound in the speakers :/ */
        return( i_buffer_limit );
    }
    return( p_aout->p_sys->i_audio_end - i_buffer_limit);
}

/*****************************************************************************
 * aout_Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
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
 * aout_Close: close the audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
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
 * aout_SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void aout_SDLCallback( void *userdata, byte_t *stream, int len )
{
    struct aout_sys_s * p_sys = userdata;

    if( p_sys->i_audio_end > OVERFLOWLIMIT )
    {
        intf_ErrMsg( "aout error: aout_SDLCallback overflowed" );

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

