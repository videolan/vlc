/*****************************************************************************
 * wav.c: wav muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("WAV muxer") );
    set_capability( "sout mux", 5 );
    set_callbacks( Open, Close );
    add_shortcut( "wav" );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Capability(sout_mux_t *, int, void *, void * );
static int  AddStream( sout_mux_t *, sout_input_t * );
static int  DelStream( sout_mux_t *, sout_input_t * );
static int  Mux      ( sout_mux_t * );

struct sout_mux_sys_t
{
    vlc_bool_t b_used;
    vlc_bool_t b_header;

    /* Wave header for the output data */
    WAVEHEADER waveheader;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;

    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->b_used       = VLC_FALSE;
    p_sys->b_header     = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    free( p_sys );
}

static int Capability( sout_mux_t *p_mux, int i_query,
                       void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_FALSE;
            return SOUT_MUX_CAP_ERR_OK;
        default:
            return SOUT_MUX_CAP_ERR_UNIMPLEMENTED;
   }
}

static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_bytes_per_sample;

    if( p_input->p_fmt->i_cat != AUDIO_ES )
        msg_Dbg( p_mux, "not an audio stream" );

    if( p_sys->b_used )
    {
        msg_Dbg( p_mux, "can't add more than 1 stream" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_mux, "adding input %i channels, %iHz",
             p_input->p_fmt->audio.i_channels,
             p_input->p_fmt->audio.i_rate );

    //p_input->p_fmt->i_codec;

    /* Build a WAV header for the output data */
    memset( &p_sys->waveheader, 0, sizeof(WAVEHEADER) );
    SetWLE( &p_sys->waveheader.Format, 1 ); /*WAVE_FORMAT_PCM*/
    SetWLE( &p_sys->waveheader.BitsPerSample, 16);
    p_sys->waveheader.MainChunkID = VLC_FOURCC('R', 'I', 'F', 'F');
    p_sys->waveheader.Length = 0;                     /* we just don't know */
    p_sys->waveheader.ChunkTypeID = VLC_FOURCC('W', 'A', 'V', 'E');
    p_sys->waveheader.SubChunkID = VLC_FOURCC('f', 'm', 't', ' ');
    SetDWLE( &p_sys->waveheader.SubChunkLength, 16 );
    SetWLE( &p_sys->waveheader.Modus, p_input->p_fmt->audio.i_channels );
    SetDWLE( &p_sys->waveheader.SampleFreq, p_input->p_fmt->audio.i_rate );
    i_bytes_per_sample = p_input->p_fmt->audio.i_channels *
        16 /*BitsPerSample*/ / 8;
    SetWLE( &p_sys->waveheader.BytesPerSample, i_bytes_per_sample );
    SetDWLE( &p_sys->waveheader.BytesPerSec,
             i_bytes_per_sample * p_input->p_fmt->audio.i_rate );
    p_sys->waveheader.DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
    p_sys->waveheader.DataLength = 0;                 /* we just don't know */

    p_sys->b_used = VLC_TRUE;

    return VLC_SUCCESS;
}

static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    return VLC_SUCCESS;
}

static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( !p_mux->i_nb_inputs ) return VLC_SUCCESS;

    if( p_sys->b_header )
    {
        /* Return only the header */
        block_t *p_block = block_New( p_mux, sizeof( WAVEHEADER ) );
        memcpy( p_block->p_buffer, &p_sys->waveheader, sizeof(WAVEHEADER) );

        msg_Dbg( p_mux, "writing header data" );
        sout_AccessOutWrite( p_mux->p_access, p_block );
    }
    p_sys->b_header = VLC_FALSE;

    while( p_mux->pp_inputs[0]->p_fifo->i_depth > 0 )
    {
        block_t *p_block = block_FifoGet( p_mux->pp_inputs[0]->p_fifo );
        sout_AccessOutWrite( p_mux->p_access, p_block );
    }

    return VLC_SUCCESS;
}
