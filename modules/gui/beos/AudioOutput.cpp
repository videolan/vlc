/*****************************************************************************
 * AudioOutput.cpp: BeOS audio output
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: AudioOutput.cpp,v 1.26 2003/01/10 16:21:39 titer Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Eric Petit <titer@videolan.org>
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
#include <malloc.h>
#include <string.h>

#include <SoundPlayer.h>
#include <media/MediaDefs.h>


#include <vlc/vlc.h>
#include <vlc/aout.h>
extern "C" {
#include <aout_internal.h>
}

/*****************************************************************************
 * aout_sys_t: BeOS audio output method descriptor
 *****************************************************************************/

typedef struct aout_sys_t
{
    BSoundPlayer *p_player;
    mtime_t       latency;
    
} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Play         ( void *p_aout, void *p_buffer, size_t size,
                           const media_raw_audio_format &format );
static void DoNothing    ( aout_instance_t *p_aout );
static int  CheckLatency ( aout_instance_t *p_aout );

/*****************************************************************************
 * OpenAudio
 *****************************************************************************/
int E_(OpenAudio) ( vlc_object_t * p_this )
{
    aout_instance_t *p_aout = (aout_instance_t*) p_this;
    p_aout->output.p_sys = (aout_sys_t *) malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "Not enough memory" );
        return -1;
    }
    aout_sys_t *p_sys = p_aout->output.p_sys;

    aout_VolumeSoftInit( p_aout );

    int i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    /* BSoundPlayer does not support more than 2 channels AFAIK */
    if ( i_nb_channels > 2 )
    {
        i_nb_channels = 2;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
 
    media_raw_audio_format *p_format;
    p_format = (media_raw_audio_format*)
        malloc( sizeof( media_raw_audio_format ) );
    
    p_format->channel_count = i_nb_channels;
    p_format->frame_rate = p_aout->output.output.i_rate;    
    p_format->format = media_raw_audio_format::B_AUDIO_FLOAT;
#ifdef WORDS_BIGENDIAN
    p_format->byte_order = B_MEDIA_BIG_ENDIAN;
#else
    p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
#endif
    p_format->buffer_size = 8192;

    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
    p_aout->output.i_nb_samples = 2048 / i_nb_channels;
    p_aout->output.pf_play = DoNothing;

    p_sys->p_player = new BSoundPlayer( p_format, "player", Play, NULL, p_aout );
    if( p_sys->p_player->InitCheck() != B_OK )
    {
        msg_Err( p_aout, "BSoundPlayer InitCheck failed" );
        delete p_sys->p_player;
        free( p_sys );
        return -1;
    }

    /* FIXME FIXME FIXME
     * We should check BSoundPlayer Latency() everytime we call
     * aout_OutputNextBuffer(). Unfortunatly, it does not seem to work
     * correctly: on my computer, it hangs for about 5 seconds at the
     * beginning of the file. This is not acceptable, so we will start
     * playing the file with a default latency (my computer's one ;p )
     * and we create a thread who's going to update it ASAP. According
     * to what I've seen, the latency is almost constant most of the
     * time anyway -- titer */
    p_sys->latency = 16209;
    if( vlc_thread_create( p_aout, "latency", CheckLatency,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create Latency thread" );
    }

    p_sys->p_player->Start();
    p_sys->p_player->SetHasData( true );

    return 0;
}

/*****************************************************************************
 * CloseAudio
 *****************************************************************************/
void E_(CloseAudio) ( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *) p_this;
    aout_sys_t * p_sys = (aout_sys_t *) p_aout->output.p_sys;
    
    p_aout->b_die = VLC_TRUE;
    vlc_thread_join( p_aout );
    p_aout->b_die = VLC_FALSE;
    
    p_sys->p_player->Stop();
    delete p_sys->p_player;
    free( p_sys );
}

/*****************************************************************************
 * Play
 *****************************************************************************/
static void Play( void *aout, void *p_buffer, size_t i_size,
                  const media_raw_audio_format &format )
{
    aout_instance_t *p_aout = (aout_instance_t*)aout;
    aout_sys_t *p_sys = (aout_sys_t*)p_aout->output.p_sys;
    aout_buffer_t * p_aout_buffer;
    mtime_t play_time = 0;
    
    /* FIXME (see above) */
    play_time = mdate() + p_sys->latency;
    
    p_aout_buffer = aout_OutputNextBuffer( p_aout, play_time, VLC_FALSE );

    if( p_aout_buffer != NULL )
    {
        p_aout->p_vlc->pf_memcpy( (float*)p_buffer,
                                  p_aout_buffer->p_buffer, i_size );
        aout_BufferFree( p_aout_buffer );
    }
    else
    {
        p_aout->p_vlc->pf_memset( (float*)p_buffer, 0, i_size );
    }
}

/*****************************************************************************
 * DoNothing 
 *****************************************************************************/
static void DoNothing( aout_instance_t *p_aout )
{
    return;
}

/*****************************************************************************
 * CheckLatency
 *****************************************************************************/
static int CheckLatency( aout_instance_t *p_aout )
{
    aout_sys_t *p_sys = (aout_sys_t*)p_aout->output.p_sys;
    mtime_t last_check = 0;
    mtime_t latency;
    
    while( !p_aout->b_die )
    {
        /* check every 0.1 second */
        if( mdate() > last_check + 100000 )
        {
            latency = p_sys->p_player->Latency();
            if( latency && latency != p_sys->latency )
            {
                p_sys->latency = latency;
            }
            last_check = mdate();
        }
        snooze( 5000 );
    }
    return VLC_SUCCESS;
}
