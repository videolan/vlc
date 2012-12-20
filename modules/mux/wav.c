/*****************************************************************************
 * wav.c: wav muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2004, 2006 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codecs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("WAV muxer") )
    set_capability( "sout mux", 5 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    set_callbacks( Open, Close )
    add_shortcut( "wav" )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

#define MAX_CHANNELS 6

struct sout_mux_sys_t
{
    bool b_used;
    bool b_header;
    bool b_ext;

    uint32_t i_data;

    /* Wave header for the output data */
    uint32_t waveheader[5];
    WAVEFORMATEXTENSIBLE waveformat;
    uint32_t waveheader2[2];

    uint32_t i_channel_mask;
    uint8_t i_chans_to_reorder;            /* do we need channel reordering */
    uint8_t pi_chan_table[AOUT_CHAN_MAX];
};

static const uint32_t pi_channels_in[] =
    { WAVE_SPEAKER_FRONT_LEFT, WAVE_SPEAKER_FRONT_RIGHT,
      WAVE_SPEAKER_SIDE_LEFT, WAVE_SPEAKER_SIDE_RIGHT,
      WAVE_SPEAKER_BACK_LEFT, WAVE_SPEAKER_BACK_RIGHT, WAVE_SPEAKER_BACK_CENTER,
      WAVE_SPEAKER_FRONT_CENTER, WAVE_SPEAKER_LOW_FREQUENCY, 0 };
static const uint32_t pi_channels_out[] =
    { WAVE_SPEAKER_FRONT_LEFT, WAVE_SPEAKER_FRONT_RIGHT,
      WAVE_SPEAKER_FRONT_CENTER, WAVE_SPEAKER_LOW_FREQUENCY,
      WAVE_SPEAKER_BACK_LEFT, WAVE_SPEAKER_BACK_RIGHT,
      WAVE_SPEAKER_BACK_CENTER,
      WAVE_SPEAKER_SIDE_LEFT, WAVE_SPEAKER_SIDE_RIGHT, 0 };

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;

    p_mux->pf_control  = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->b_used   = false;
    p_sys->b_header = true;
    p_sys->i_data   = 0;

    p_sys->i_chans_to_reorder = 0;

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

static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;
    char **ppsz;

    switch( i_query )
    {
        case MUX_CAN_ADD_STREAM_WHILE_MUXING:
            pb_bool = (bool*)va_arg( args, bool * );
            *pb_bool = false;
            return VLC_SUCCESS;

        case MUX_GET_ADD_STREAM_WAIT:
            pb_bool = (bool*)va_arg( args, bool * );
            *pb_bool = true;
            return VLC_SUCCESS;

        case MUX_GET_MIME:
            ppsz = (char**)va_arg( args, char ** );
            *ppsz = strdup( "audio/wav" );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    GUID subformat_guid = {0, 0, 0x10,{0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    WAVEFORMATEX *p_waveformat = &p_sys->waveformat.Format;
    int i_bytes_per_sample;
    uint16_t i_format;
    bool b_ext;

    if( p_input->p_fmt->i_cat != AUDIO_ES )
    {
        msg_Dbg( p_mux, "not an audio stream" );
        return VLC_EGENERIC;
    }

    if( p_sys->b_used )
    {
        msg_Dbg( p_mux, "can't add more than 1 stream" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_mux, "adding %i input channels, %iHz",
             p_input->p_fmt->audio.i_channels,
             p_input->p_fmt->audio.i_rate );

    p_sys->i_channel_mask = 0;
    if( p_input->p_fmt->audio.i_physical_channels )
    {
        for( unsigned i = 0; i < pi_vlc_chan_order_wg4[i]; i++ )
            if( p_input->p_fmt->audio.i_physical_channels & pi_vlc_chan_order_wg4[i])
                p_sys->i_channel_mask |= pi_channels_in[i];

        p_sys->i_chans_to_reorder =
            aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                      p_sys->i_channel_mask,
                                      p_sys->pi_chan_table );

        msg_Dbg( p_mux, "channel mask: %x, reordering: %u",
                 p_sys->i_channel_mask, p_sys->i_chans_to_reorder );
    }

    fourcc_to_wf_tag( p_input->p_fmt->i_codec, &i_format );
    b_ext = p_sys->b_ext = p_input->p_fmt->audio.i_channels > 2;

    /* Build a WAV header for the output data */
    p_sys->waveheader[0] = VLC_FOURCC('R', 'I', 'F', 'F'); /* MainChunkID */
    SetDWLE( &p_sys->waveheader[1], 0 ); /* Length */
    p_sys->waveheader[2] = VLC_FOURCC('W', 'A', 'V', 'E'); /* ChunkTypeID */
    p_sys->waveheader[3] = VLC_FOURCC('f', 'm', 't', ' '); /* SubChunkID */
    SetDWLE( &p_sys->waveheader[4], b_ext ? 40 : 16 ); /* SubChunkLength */

    p_sys->waveheader2[0] = VLC_FOURCC('d', 'a', 't', 'a'); /* DataChunkID */
    SetDWLE( &p_sys->waveheader2[1], 0 ); /* DataLength */

    /* Build a WAVEVFORMAT header for the output data */
    memset( &p_sys->waveformat, 0, sizeof(WAVEFORMATEXTENSIBLE) );
    SetWLE( &p_waveformat->wFormatTag,
            b_ext ? WAVE_FORMAT_EXTENSIBLE : i_format );
    SetWLE( &p_waveformat->nChannels,
            p_input->p_fmt->audio.i_channels );
    SetDWLE( &p_waveformat->nSamplesPerSec, p_input->p_fmt->audio.i_rate );
    i_bytes_per_sample = p_input->p_fmt->audio.i_channels *
        p_input->p_fmt->audio.i_bitspersample / 8;
    SetDWLE( &p_waveformat->nAvgBytesPerSec,
             i_bytes_per_sample * p_input->p_fmt->audio.i_rate );
    SetWLE( &p_waveformat->nBlockAlign, i_bytes_per_sample );
    SetWLE( &p_waveformat->wBitsPerSample,
            p_input->p_fmt->audio.i_bitspersample );
    SetWLE( &p_waveformat->cbSize, 22 );
    SetWLE( &p_sys->waveformat.Samples.wValidBitsPerSample,
            p_input->p_fmt->audio.i_bitspersample );
    SetDWLE( &p_sys->waveformat.dwChannelMask,
             p_sys->i_channel_mask );
    p_sys->waveformat.SubFormat = subformat_guid;
    p_sys->waveformat.SubFormat.Data1 = i_format;


    p_sys->b_used = true;

    return VLC_SUCCESS;
}

static block_t *GetHeader( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t *p_block =
        block_Alloc( sizeof( WAVEFORMATEXTENSIBLE ) + 7 * 4 );

    SetDWLE( &p_sys->waveheader[1],
             20 + (p_sys->b_ext ? 40 : 16) + p_sys->i_data ); /* Length */
    SetDWLE( &p_sys->waveheader2[1], p_sys->i_data ); /* DataLength */

    memcpy( p_block->p_buffer, &p_sys->waveheader, 5 * 4 );
    memcpy( p_block->p_buffer + 5 * 4, &p_sys->waveformat,
            sizeof( WAVEFORMATEXTENSIBLE ) );
    memcpy( p_block->p_buffer + 5 * 4 +
            (p_sys->b_ext ? sizeof( WAVEFORMATEXTENSIBLE ) : 16),
            &p_sys->waveheader2, 2 * 4 );
    if( !p_sys->b_ext ) p_block->i_buffer -= 24;
    return p_block;
}

static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    VLC_UNUSED(p_input);
    msg_Dbg( p_mux, "removing input" );

    msg_Dbg( p_mux, "writing header data" );
    if( sout_AccessOutSeek( p_mux->p_access, 0 ) == VLC_SUCCESS )
    {
        sout_AccessOutWrite( p_mux->p_access, GetHeader( p_mux ) );
    }

    return VLC_SUCCESS;
}

static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    sout_input_t *p_input;

    if( !p_mux->i_nb_inputs ) return VLC_SUCCESS;

    if( p_sys->b_header )
    {
        msg_Dbg( p_mux, "writing header data" );
        sout_AccessOutWrite( p_mux->p_access, GetHeader( p_mux ) );
    }
    p_sys->b_header = false;

    p_input = p_mux->pp_inputs[0];
    while( block_FifoCount( p_input->p_fifo ) > 0 )
    {
        block_t *p_block = block_FifoGet( p_input->p_fifo );
        p_sys->i_data += p_block->i_buffer;

        /* Do the channel reordering */
        if( p_sys->i_chans_to_reorder )
            aout_ChannelReorder( p_block->p_buffer, p_block->i_buffer,
                                 p_sys->i_chans_to_reorder,
                                 p_sys->pi_chan_table, p_input->p_fmt->i_codec );

        sout_AccessOutWrite( p_mux->p_access, p_block );
    }

    return VLC_SUCCESS;
}
