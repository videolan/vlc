/*****************************************************************************
 * aout.cpp: BeOS audio output
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: AudioOutput.cpp,v 1.2 2002/08/17 08:46:46 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <stdio.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <kernel/OS.h>
#include <View.h>
#include <Application.h>
#include <Message.h>
#include <Locker.h>
#include <media/MediaDefs.h>
#include <game/PushGameSound.h>
#include <malloc.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

/*****************************************************************************
 * aout_sys_t: BeOS audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some BeOS specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    BPushGameSound   * p_sound;
    gs_audio_format  * p_format;
    void             * p_buffer;
    int 	         i_buffer_size;
    int              i_buffer_pos;
    mtime_t          clock_diff;

};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     SetFormat   ( aout_instance_t * );
//static int     GetBufInfo  ( aout_instance_t *, int );
static void    Play        ( aout_instance_t *, aout_buffer_t * );

/*****************************************************************************
 * OpenAudio: opens a BPushGameSound
 *****************************************************************************/
int E_(OpenAudio) ( vlc_object_t * p_this )
{       

    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;

    /* Allocate instance */
    p_sys = p_aout->output.p_sys = (aout_sys_t *)malloc( sizeof( struct aout_sys_t ) );
    memset( p_sys, 0, sizeof( struct aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( 1 );
    }

    /* Allocate BPushGameSound */
    p_sys->p_sound = new BPushGameSound( 8192,
                                         p_sys->p_format,
                                         2, NULL );
    if( p_sys->p_sound == NULL )
    {
        free( p_sys->p_format );
        free( p_sys );
        msg_Err( p_aout, "cannot allocate BPushGameSound" );
        return( 1 );
    }

    if( p_sys->p_sound->InitCheck() != B_OK )
    {
        free( p_sys->p_format );
        free( p_sys );
        msg_Err( p_aout, "cannot initialize BPushGameSound" );
        return( 1 );
    }

    p_sys->p_sound->StartPlaying( );

    p_sys->p_sound->LockForCyclic( &p_sys->p_buffer,
                                   (size_t *)&p_sys->i_buffer_size );

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;
    return( 0 );
}

/*****************************************************************************
 * SetFormat: sets the dsp output format
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
    /* Initialize some variables */
    p_aout->output.p_sys->p_format->frame_rate = p_aout->output.output.i_rate;
    p_aout->output.p_sys->p_format->channel_count = p_aout->output.output.i_channels;
    p_aout->output.p_sys->p_format->format = gs_audio_format::B_GS_S16;
    p_aout->output.p_sys->p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
    p_aout->output.p_sys->p_format->buffer_size = 4*8192;
    p_aout->output.p_sys->i_buffer_pos = 0;
    msg_Err( p_aout, "Rate: %d, Channels: %d", p_aout->output.output.i_rate, p_aout->output.output.i_channels);

//    p_aout->output.pf_getbufinfo = GetBufInfo;
    return( 0 );
}

/*****************************************************************************
 * Play: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void Play( aout_instance_t *p_aout,
                  aout_buffer_t *p_buffer )
{
    int i_newbuf_pos;

    if( (i_newbuf_pos = p_aout->output.p_sys->i_buffer_pos + p_buffer->i_size)
              > p_aout->output.p_sys->i_buffer_size )
    {
        p_aout->p_vlc->pf_memcpy( (void *)((int)p_aout->output.p_sys->p_buffer
                        + p_aout->output.p_sys->i_buffer_pos),
                p_buffer->p_buffer,
                p_aout->output.p_sys->i_buffer_size - p_aout->output.p_sys->i_buffer_pos );

        p_aout->p_vlc->pf_memcpy( (void *)((int)p_aout->output.p_sys->p_buffer),
                p_buffer->p_buffer + p_aout->output.p_sys->i_buffer_size - p_aout->output.p_sys->i_buffer_pos,
                p_buffer->i_size - ( p_aout->output.p_sys->i_buffer_size
                             - p_aout->output.p_sys->i_buffer_pos ) );
        
        p_aout->output.p_sys->i_buffer_pos = i_newbuf_pos - p_aout->output.p_sys->i_buffer_size;

    }
    else
    {
       p_aout->p_vlc->pf_memcpy( (void *)((int)p_aout->output.p_sys->p_buffer + p_aout->output.p_sys->i_buffer_pos),
                p_buffer->p_buffer, p_buffer->i_size );

       p_aout->output.p_sys->i_buffer_pos = i_newbuf_pos;
    }
}

/*****************************************************************************
 * CloseAudio: closes the dsp audio device
 *****************************************************************************/
void E_(CloseAudio) ( vlc_object_t *p_this )
{       
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    p_aout->output.p_sys->p_sound->UnlockCyclic();
    p_aout->output.p_sys->p_sound->StopPlaying( );
    delete p_aout->output.p_sys->p_sound;
    free( p_aout->output.p_sys->p_format );
    free( p_aout->output.p_sys );
}

/*****************************************************************************
 * SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void BeOSCallback( void * _p_aout, byte_t * p_stream, int i_len )
{
}
