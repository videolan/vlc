/*****************************************************************************
 * AudioOutput.cpp: BeOS audio output
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: AudioOutput.cpp,v 1.17 2002/11/27 05:36:41 titer Exp $
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
#include <aout_internal.h>

#define FRAME_SIZE  2048
#define BUFFER_SIZE 16384

/*****************************************************************************
 * aout_sys_t: BeOS audio output method descriptor
 *****************************************************************************/

typedef struct aout_sys_t
{
    BSoundPlayer *p_player;
    
} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Play         ( void *p_aout, void *p_buffer, size_t size,
                           const media_raw_audio_format &format );
static void DoNothing    ( aout_instance_t *p_aout );

/*****************************************************************************
 * OpenAudio
 *****************************************************************************/
int E_(OpenAudio) ( vlc_object_t * p_this )
{
    int i_nb_channels;
    aout_instance_t *p_aout = (aout_instance_t*) p_this;
    p_aout->output.p_sys = (aout_sys_t *) malloc( sizeof( aout_sys_t ) );

    aout_sys_t *p_sys = p_aout->output.p_sys;

    aout_VolumeSoftInit( p_aout );
    
    media_raw_audio_format *p_format;
    p_format = (media_raw_audio_format*)
        malloc( sizeof( media_raw_audio_format ) );
    
    p_format->frame_rate = p_aout->output.output.i_rate;

    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels > 2 )
    {
        /* BSoundPlayer does not support more than 2 channels AFAIK */
        i_nb_channels = 2;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    p_format->channel_count = i_nb_channels;

    p_format->format = media_raw_audio_format::B_AUDIO_FLOAT;
#ifdef WORDS_BIGENDIAN
    p_format->byte_order = B_MEDIA_BIG_ENDIAN;
#else
    p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
#endif
    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');

    p_format->buffer_size = BUFFER_SIZE;
    p_aout->output.i_nb_samples = FRAME_SIZE;
    p_aout->output.pf_play = DoNothing;
    
    p_sys->p_player = new BSoundPlayer( p_format, "player",
                                        Play, NULL, p_aout );
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
    aout_buffer_t * p_aout_buffer;
    aout_instance_t *p_aout = (aout_instance_t*) aout;

    p_aout_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );

    if( p_aout_buffer != NULL )
    {
        /* sometimes p_aout_buffer is not NULL but still isn't valid.
           we check i_nb_bytes so we are sure it is */
        if( p_aout_buffer->i_nb_bytes == BUFFER_SIZE )
        {
           memcpy( (float*)p_buffer,
                   p_aout_buffer->p_buffer,
                   BUFFER_SIZE );
           aout_BufferFree( p_aout_buffer );
        }
    }
}

/*****************************************************************************
 * DoNothing 
 *****************************************************************************/
static void DoNothing( aout_instance_t *p_aout)
{
    return;
}
