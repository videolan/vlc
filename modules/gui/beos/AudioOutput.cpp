/*****************************************************************************
 * aout.cpp: BeOS audio output
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: AudioOutput.cpp,v 1.10 2002/10/12 12:24:52 titer Exp $
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

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

/*****************************************************************************
 * aout_sys_t: BeOS audio output method descriptor
 *****************************************************************************/

typedef struct aout_sys_t
{
    BSoundPlayer *p_player;
    float *p_buffer;
    size_t i_buffer_size;
} aout_sys_t;

#define FRAME_SIZE 2048

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
	aout_instance_t *p_aout = (aout_instance_t*) p_this;
	p_aout->output.p_sys = (aout_sys_t *) malloc( sizeof( aout_sys_t ) );
	
	aout_sys_t *p_sys = p_aout->output.p_sys;
    p_sys->i_buffer_size = 0;
    p_sys->p_buffer = NULL;

    aout_VolumeSoftInit( p_aout );
    
    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
    p_aout->output.i_nb_samples = FRAME_SIZE;
    p_aout->output.pf_play = DoNothing;
    p_aout->output.output.i_rate = 44100;
    p_aout->output.output.i_channels = 2;

    p_sys->p_player = new BSoundPlayer( "player", Play, NULL, p_this );
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
    p_aout->b_die = 1;
    delete p_sys->p_player;
    free( p_sys );
}

/*****************************************************************************
 * Play
 *****************************************************************************/
static void Play( void *aout, void *buffer, size_t size,
                  const media_raw_audio_format &format )
{
    aout_buffer_t * p_aout_buffer;
    aout_instance_t *p_aout = (aout_instance_t*) aout;
    aout_sys_t *p_sys = p_aout->output.p_sys;

    float *p_buffer = (float*) buffer;
    
    if( format.format != media_raw_audio_format::B_AUDIO_FLOAT )
    {
        msg_Err( p_aout, "Bad audio format" );
        return;
    }

    if( p_sys->i_buffer_size < size )
    {
        p_aout_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
        if( p_aout_buffer != NULL )
        {
            if( p_sys->p_buffer == NULL )
            {
                p_sys->p_buffer = (float*)malloc( p_aout_buffer->i_nb_bytes );
                p_sys->i_buffer_size = p_aout_buffer->i_nb_bytes;
            }
            else
            {
                realloc( p_sys->p_buffer,
                         p_sys->i_buffer_size + p_aout_buffer->i_nb_bytes );
                memcpy( p_sys->p_buffer + p_sys->i_buffer_size,
                        p_aout_buffer->p_buffer,
                        p_aout_buffer->i_nb_bytes );
                p_sys->i_buffer_size += p_aout_buffer->i_nb_bytes;
            }
        }
    }

    if( p_sys->i_buffer_size >= size )
    {
        memcpy( p_buffer, p_sys->p_buffer,
                MIN( size, p_sys->i_buffer_size ) );
        p_sys->i_buffer_size -=  MIN( size, p_sys->i_buffer_size );
    }
}

/*****************************************************************************
 * DoNothing 
 *****************************************************************************/
static void DoNothing( aout_instance_t *p_aout)
{
    return;
}
