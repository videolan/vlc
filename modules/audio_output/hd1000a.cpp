/*****************************************************************************
 * hd1000a.cpp : Roku HD1000 audio output
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Author: Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
extern "C"
{
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include "aout_internal.h"
}

#include <deschutes/libraries/hdmachinex225/PCMAudioPlayer.h>

#define FRAME_SIZE 4096

/*****************************************************************************
 * aout_sys_t: audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    u32 nAlignment;
    u32 nSizeMultiple;
    u32 nBuffers;
    u32 nBufferSize;
    void ** ppBuffers;
    u32 nNextBufferIndex;
    PCMAudioPlayer * pPlayer;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     Open        ( vlc_object_t * );
static void    Close       ( vlc_object_t * );

static void    Play        ( aout_instance_t * );
static int     Thread      ( aout_instance_t * );

static void    InterleaveS16( int16_t *, int16_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "Roku HD1000" );
    set_description( N_("Roku HD1000 audio output") );
    set_capability( "audio output", 100 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open a dummy audio device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    PCMAudioPlayer * pPlayer;
    int i_volume;

    /* Allocate structure */
    p_aout->output.p_sys = p_sys =
        (aout_sys_t *)malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_EGENERIC;
    }

    /* New PCMAudioPlayer */
    p_sys->pPlayer = pPlayer = new PCMAudioPlayer();
    if( p_sys->pPlayer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Get Buffer Requirements */
    if( !pPlayer->GetBufferRequirements( p_sys->nAlignment,
                                         p_sys->nSizeMultiple,
                                         p_sys->nBuffers ) )
    {
        msg_Err( p_aout, "GetBufferRequirements failed" );
        delete pPlayer;
        free( p_sys );
        return VLC_EGENERIC;
    } 

    p_sys->nBuffers = __MIN( p_sys->nBuffers, 4 );

    p_sys->ppBuffers = (void **)malloc( p_sys->nBuffers * sizeof( void * ) );
    if( p_sys->ppBuffers == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        delete pPlayer;
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Open PCMAudioPlayer */
    p_sys->nBufferSize = FRAME_SIZE * 4;
    if( !pPlayer->Open( p_sys->nBuffers, p_sys->nBufferSize,
                        p_sys->ppBuffers ) )
    {
        msg_Err( p_aout, "Open failed" );
        delete pPlayer;
        free( p_sys->ppBuffers );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->nNextBufferIndex = 0;

    if( !pPlayer->SetSampleRate( p_aout->output.output.i_rate ) )
    {
        p_aout->output.output.i_rate = 44100;
        if( !pPlayer->SetSampleRate( p_aout->output.output.i_rate ) )
        {
            msg_Err( p_aout, "SetSampleRate failed" );
            pPlayer->Close();
            delete pPlayer;
            free( p_sys->ppBuffers );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    p_aout->output.i_nb_samples = FRAME_SIZE;
    p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    i_volume = config_GetInt( p_aout->p_vlc, "volume" );
    pPlayer->SetVolume( (u32)__MIN( i_volume * 64, 0xFFFF ) );

    /* Create thread and wait for its readiness. */
    if( vlc_thread_create( p_aout, "aout", Thread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create OSS thread (%s)", strerror(errno) );
        pPlayer->Close();
        delete pPlayer;
        free( p_sys->ppBuffers );
        free( p_sys );
        return VLC_ETHREAD;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close our file
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    u32 i;
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_aout->b_die = VLC_TRUE;
    vlc_thread_join( p_aout );
    p_aout->b_die = VLC_FALSE;

    do
    {
        i = p_sys->pPlayer->WaitForBuffer();
    } while( i != 0 && i != p_sys->nBuffers );

    p_sys->pPlayer->Close();
    delete p_sys->pPlayer;

    free( p_sys->ppBuffers );
    free( p_sys );
}

/*****************************************************************************
 * Play: do nothing
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}

/*****************************************************************************
 * Thread: thread used to DMA the data to the device
 *****************************************************************************/
static int Thread( aout_instance_t * p_aout )
{
    aout_buffer_t * p_buffer;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    PCMAudioPlayer * pPlayer = p_sys->pPlayer;

    while( !p_aout->b_die )
    {
        pPlayer->WaitForBuffer();

        vlc_mutex_lock( &p_aout->output_fifo_lock );
        p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
        vlc_mutex_unlock( &p_aout->output_fifo_lock );

#define i p_sys->nNextBufferIndex
        if( p_buffer == NULL )
        {
            p_aout->p_vlc->pf_memset( p_sys->ppBuffers[ i ], 0,
                                      p_sys->nBufferSize ); 
        }
        else
        {
            InterleaveS16( (int16_t *)p_buffer->p_buffer,
                           (int16_t *)p_sys->ppBuffers[ i ] );
            aout_BufferFree( p_buffer );
        }

        if( !pPlayer->QueueBuffer( (s16 *)p_sys->ppBuffers[ i ],
                                   p_sys->nBufferSize / 2 ) )
        {
            msg_Err( p_aout, "QueueBuffer failed" );
        } 

        i = (i + 1) % p_sys->nBuffers;
#undef i
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InterleaveS16: interleave samples
 *****************************************************************************/
static void InterleaveS16( int16_t * p_in, int16_t * p_out )
{
    for( int i = 0; i < FRAME_SIZE; i++ )
    {
        p_out[ i * 2 + 0 ] = p_in[ i * 2 + 1 ];
        p_out[ i * 2 + 1 ] = p_in[ i * 2 + 0 ];
    }
}
