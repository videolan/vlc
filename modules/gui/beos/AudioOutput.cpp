/*****************************************************************************
 * aout.cpp: BeOS audio output
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: AudioOutput.cpp,v 1.7 2002/08/30 23:27:06 massiot Exp $
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
    uint             i_buffer_pos;
    mtime_t          clock_diff;
};

#define FRAME_SIZE 2048

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  SetFormat    ( aout_instance_t * );
static void Play         ( aout_instance_t * );
static int  BeOSThread   ( aout_instance_t * );

/*****************************************************************************
 * OpenAudio: opens a BPushGameSound
 *****************************************************************************/
int E_(OpenAudio) ( vlc_object_t * p_this )
{       
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;

    /* Allocate structure */
    p_aout->output.p_sys = p_sys = (aout_sys_t *)malloc( sizeof( struct aout_sys_t ) );
    memset( p_sys, 0, sizeof( struct aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }
    
    /* Initialize format */
    p_sys->p_format = (gs_audio_format *)malloc( sizeof( struct gs_audio_format));
    SetFormat(p_aout);

    /* Allocate BPushGameSound */
    p_sys->p_sound = new BPushGameSound( 8192,
                                         p_sys->p_format,
                                         2, NULL );
    if( p_sys->p_sound->InitCheck() != B_OK )
    {
        free( p_sys->p_format );
        free( p_sys );
        msg_Err( p_aout, "cannot initialize BPushGameSound" );
        return -1;
    }

    if( vlc_thread_create( p_aout, "aout", BeOSThread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create aout thread" );
        delete p_sys->p_sound;
        free( p_sys->p_format );
        free( p_sys );
        return -1;
    }
    
    p_aout->output.pf_play = Play;

    return 0;
}

/*****************************************************************************
 * SetFormat: sets the dsp output format
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
    /* Initialize some variables */
    p_aout->output.p_sys->p_format->frame_rate = p_aout->output.output.i_rate;
    p_aout->output.p_sys->p_format->channel_count = p_aout->output.output.i_channels;
    
    switch (p_aout->output.output.i_format)
    {
    case AOUT_FMT_S16_LE:
        p_aout->output.p_sys->p_format->format = gs_audio_format::B_GS_S16;
        p_aout->output.p_sys->p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
        break;
    case AOUT_FMT_S16_BE:
        p_aout->output.p_sys->p_format->format = gs_audio_format::B_GS_S16;
        p_aout->output.p_sys->p_format->byte_order = B_MEDIA_BIG_ENDIAN;
        break;
    case AOUT_FMT_S8:
        p_aout->output.p_sys->p_format->format = gs_audio_format::B_GS_U8;
        p_aout->output.p_sys->p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
        break;
    case AOUT_FMT_FLOAT32:
        p_aout->output.p_sys->p_format->format = gs_audio_format::B_GS_F;
        p_aout->output.p_sys->p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
        break;
    default:
        msg_Err( p_aout, "cannot set audio format (%i)",
                          p_aout->output.output.i_format );
        return -1;
    }

    p_aout->output.p_sys->p_format->buffer_size = 4*8192;

    return( 0 );
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
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
    
    p_aout->b_die = 1;
    free( p_aout->output.p_sys->p_format );
    free( p_aout->output.p_sys );
}

/*****************************************************************************
 * GetBufInfo: buffer status query
 *****************************************************************************
 * This function fills in the audio_buf_info structure :
 * - returns : number of available fragments (not partially used ones)
 * - int fragstotal : total number of fragments allocated
 * - int fragsize : size of a fragment in bytes
 * - int bytes : available space in bytes (includes partially used fragments)
 * Note! 'bytes' could be more than fragments*fragsize
 *****************************************************************************/
static int GetBufInfo( aout_instance_t * p_aout )
{
    /* returns the allocated space in bytes */
    return ( p_aout->output.p_sys->p_format->buffer_size );
}

/*****************************************************************************
 * BeOSThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int BeOSThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    static uint i_buffer_pos;

    p_sys->p_sound->StartPlaying( );
    p_sys->p_sound->LockForCyclic( &p_sys->p_buffer,
                                   (size_t *)&p_sys->i_buffer_size );

    while ( !p_aout->b_die )
    {
        aout_buffer_t * p_buffer;
        int i_tmp, i_size;
        byte_t * p_bytes;

        mtime_t next_date = 0;
        /* Get the presentation date of the next write() operation. It
         * is equal to the current date + duration of buffered samples.
         * Order is important here, since GetBufInfo is believed to take
         * more time than mdate(). */
        next_date = (mtime_t)GetBufInfo( p_aout ) * 1000000
                  / p_aout->output.output.i_bytes_per_frame
                  / p_aout->output.output.i_rate
                  * p_aout->output.output.i_frame_length;
        next_date += mdate();

        p_buffer = aout_OutputNextBuffer( p_aout, next_date, VLC_FALSE );
 
        int i_newbuf_pos;
        if ( p_buffer != NULL )
        {
            p_bytes = p_buffer->p_buffer;
            i_size = p_buffer->i_nb_bytes;
        }
        else
        {
            i_size = FRAME_SIZE / p_aout->output.output.i_frame_length
                      * p_aout->output.output.i_bytes_per_frame;
            p_bytes = (byte_t *)malloc( i_size );
            memset( p_bytes, 0, i_size );
        }
   
        if( (i_newbuf_pos = i_buffer_pos + p_buffer->i_size)
                            > p_aout->output.p_sys->i_buffer_size )
        {
            p_aout->p_vlc->pf_memcpy( (void *)((int)p_aout->output.p_sys->p_buffer
                            + i_buffer_pos),
                              p_buffer->p_buffer,
                              p_aout->output.p_sys->i_buffer_size - i_buffer_pos );

            p_aout->p_vlc->pf_memcpy( (void *)((int)p_aout->output.p_sys->p_buffer),
                            p_buffer->p_buffer + p_aout->output.p_sys->i_buffer_size - i_buffer_pos,
                            p_buffer->i_size - ( p_aout->output.p_sys->i_buffer_size - i_buffer_pos ) );
        
            i_buffer_pos = i_newbuf_pos - i_buffer_pos;
        }
        else
        {
           p_aout->p_vlc->pf_memcpy( (void *)((int)p_aout->output.p_sys->p_buffer + i_buffer_pos),
                    p_buffer->p_buffer, p_buffer->i_size );

           i_buffer_pos = i_newbuf_pos;
        }
    }
    
    return 0;

}

