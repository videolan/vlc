/*****************************************************************************
 * aout_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 * Samuel Hocevar <sam@via.ecp.fr>
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

#include <stdio.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <kernel/OS.h>
#include <View.h>
#include <Application.h>
#include <Message.h>
#include <Locker.h>
#include <media/MediaDefs.h>
#include <game/PushGameSound.h>
#include <malloc.h>
#include <string.h>

extern "C"
{
#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "audio_output.h"

#include "intf_msg.h"

#include "main.h"
}

/*****************************************************************************
 * aout_sys_t: esd audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some esd specific variables.
 *****************************************************************************/
typedef struct aout_sys_s
{
    BPushGameSound * p_sound;
    gs_audio_format * p_format;
    void * p_buffer;
    long i_buffer_size;
    long i_buffer_pos;

} aout_sys_t;

extern "C"
{

/*****************************************************************************
 * aout_BeOpen: opens a BPushGameSound
 *****************************************************************************/
int aout_BeOpen( aout_thread_t *p_aout )
{
    /* Allocate structure */
    p_aout->p_sys = (aout_sys_t*) malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }

    /* Allocate gs_audio_format */
    p_aout->p_sys->p_format = (gs_audio_format *) malloc( sizeof( gs_audio_format ) );
    if( p_aout->p_sys->p_format == NULL )
    {
        free( p_aout->p_sys );
        intf_ErrMsg("error: cannot allocate memory for gs_audio_format\n" );
        return( 1 );
    }

    /* Initialize some variables */
    p_aout->i_format = AOUT_DEFAULT_FORMAT;
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR,
                                                  AOUT_STEREO_DEFAULT );
    p_aout->l_rate = main_GetIntVariable( AOUT_RATE_VAR, AOUT_RATE_DEFAULT );

    p_aout->p_sys->p_format->frame_rate = 44100.0;
    p_aout->p_sys->p_format->channel_count = p_aout->i_channels;
    p_aout->p_sys->p_format->format = gs_audio_format::B_GS_S16;
    p_aout->p_sys->p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
    p_aout->p_sys->p_format->buffer_size = 8192;
    p_aout->p_sys->i_buffer_pos = 0;

    /* Allocate BPushGameSound */
    p_aout->p_sys->p_sound = new BPushGameSound( 8192,
                                                 p_aout->p_sys->p_format,
                                                 2, NULL );
    if( p_aout->p_sys->p_sound == NULL )
    {
        free( p_aout->p_sys->p_format );
        free( p_aout->p_sys );
        intf_ErrMsg("error: cannot allocate memory for BPushGameSound\n" );
        return( 1 );
    }

    if( p_aout->p_sys->p_sound->InitCheck() != B_OK )
    {
        free( p_aout->p_sys->p_format );
        free( p_aout->p_sys );
        intf_ErrMsg("error: cannot allocate memory for BPushGameSound\n" );
        return( 1 );
    }

    p_aout->p_sys->p_sound->StartPlaying( );

    p_aout->p_sys->p_sound->LockForCyclic( &p_aout->p_sys->p_buffer,
                            (size_t *)&p_aout->p_sys->i_buffer_size );

    return( 0 );
}
/*****************************************************************************
 * aout_BeReset: resets the dsp
 *****************************************************************************/
int aout_BeReset( aout_thread_t *p_aout )
{
    return( 0 );
}

/*****************************************************************************
 * aout_BeSetFormat: sets the dsp output format
 *****************************************************************************/
int aout_BeSetFormat( aout_thread_t *p_aout )
{
    return( 0 );
}

/*****************************************************************************
 * aout_BeSetChannels: sets the dsp's stereo or mono mode
 *****************************************************************************/
int aout_BeSetChannels( aout_thread_t *p_aout )
{
    return( 0 );
}

/*****************************************************************************
 * aout_BeSetRate: sets the dsp's audio output rate
 *****************************************************************************/
int aout_BeSetRate( aout_thread_t *p_aout )
{
    return( 0 );
}

/*****************************************************************************
 * aout_BeGetBufInfo: buffer status query
 *****************************************************************************/
long aout_BeGetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{

    long i_hard_pos = 4 * p_aout->p_sys->p_sound->CurrentPosition();

    /*fprintf( stderr, "read 0x%.6lx - write 0x%.6lx = ",
             i_hard_pos, p_aout->p_sys->i_buffer_pos );*/

    if( i_hard_pos < p_aout->p_sys->i_buffer_pos )
    {
        i_hard_pos += p_aout->p_sys->i_buffer_size;
    }

    /*fprintf( stderr, "0x%.6lx\n", i_hard_pos - p_aout->p_sys->i_buffer_pos ); */

    return( (p_aout->p_sys->i_buffer_size - (i_hard_pos - p_aout->p_sys->i_buffer_pos)) );
}

/*****************************************************************************
 * aout_BePlaySamples: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
void aout_BePlaySamples( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    long i_newbuf_pos;

    //fprintf( stderr, "writing %i\n", i_size );

    if( (i_newbuf_pos = p_aout->p_sys->i_buffer_pos + i_size)
              > p_aout->p_sys->i_buffer_size )
    {
        memcpy( (void *)((int)p_aout->p_sys->p_buffer
                        + p_aout->p_sys->i_buffer_pos),
                buffer,
                p_aout->p_sys->i_buffer_size - p_aout->p_sys->i_buffer_pos );

        memcpy( (void *)((int)p_aout->p_sys->p_buffer),
                buffer,
                i_size - ( p_aout->p_sys->i_buffer_size
                             - p_aout->p_sys->i_buffer_pos ) );

        p_aout->p_sys->i_buffer_pos = i_newbuf_pos - p_aout->p_sys->i_buffer_size;
    }
    else
    {
        memcpy( (void *)((int)p_aout->p_sys->p_buffer + p_aout->p_sys->i_buffer_pos),
                buffer, i_size );
        p_aout->p_sys->i_buffer_pos = i_newbuf_pos;
    }
}

/*****************************************************************************
 * aout_BeClose: closes the dsp audio device
 *****************************************************************************/
void aout_BeClose( aout_thread_t *p_aout )
{
    p_aout->p_sys->p_sound->UnlockCyclic();
    p_aout->p_sys->p_sound->StopPlaying( );
    delete p_aout->p_sys->p_sound;
    free( p_aout->p_sys->p_format );
    free( p_aout->p_sys );
}

} /* extern "C" */

